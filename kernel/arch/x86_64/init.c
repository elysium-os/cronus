#include "sys/init.h"

#include "abi/sysv/elf.h"
#include "arch/cpu.h"
#include "arch/page.h"
#include "arch/ptm.h"
#include "arch/sched.h"
#include "common/assert.h"
#include "common/log.h"
#include "fs/vfs.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "memory/heap.h"
#include "memory/hhdm.h"
#include "memory/page.h"
#include "memory/pmm.h"
#include "memory/vm.h"
#include "sched/sched.h"
#include "sys/event.h"
#include "sys/init.h"
#include "sys/time.h"
#include "x86_64/abi/sysv/auxv.h"
#include "x86_64/abi/sysv/sysv.h"
#include "x86_64/cpu/cpu.h"
#include "x86_64/cpu/cpuid.h"
#include "x86_64/cpu/cr.h"
#include "x86_64/cpu/gdt.h"
#include "x86_64/cpu/lapic.h"
#include "x86_64/cpu/msr.h"
#include "x86_64/cpu/pat.h"
#include "x86_64/dev/hpet.h"
#include "x86_64/dev/ioapic.h"
#include "x86_64/dev/pic8259.h"
#include "x86_64/dev/pit.h"
#include "x86_64/interrupt.h"

#include <stddef.h>
#include <stdint.h>
#include <tartarus.h>
#include <uacpi/tables.h>
#include <uacpi/uacpi.h>

uintptr_t g_hhdm_offset;
size_t g_hhdm_size;

x86_64_cpu_t *g_x86_64_cpus;

page_t *g_page_cache;
size_t g_page_cache_size;

static volatile bool g_init_ap_finished = false;
static uint64_t g_init_ap_cpu_id = 0;

static x86_64_cpu_t g_early_bsp;

static bool g_hpet_initialized = false;

static time_frequency_t calibrate_lapic_timer() {
    uint32_t nominal_freq = 0;
    if(!x86_64_cpuid_register(0x15, X86_64_CPUID_REGISTER_ECX, &nominal_freq) && nominal_freq != 0) {
        return (time_frequency_t) nominal_freq;
    }

    x86_64_lapic_timer_setup(X86_64_LAPIC_TIMER_TYPE_ONESHOT, true, 0xFF, X86_64_LAPIC_TIMER_DIVISOR_16);
    if(!g_hpet_initialized) {
        for(size_t sample_count = 8;; sample_count *= 2) {
            x86_64_pit_set_reload(0xFFF0);
            uint16_t start_count = x86_64_pit_count();
            x86_64_lapic_timer_start(sample_count);
            while(x86_64_lapic_timer_read() != 0) cpu_relax();
            uint64_t delta = start_count - x86_64_pit_count();

            if(delta < 0x4000) continue;

            x86_64_lapic_timer_stop();
            return (sample_count / MATH_MAX(1lu, delta)) * X86_64_PIT_BASE_FREQ;
        }
    } else {
        time_t timeout = (TIME_NANOSECONDS_IN_SECOND / TIME_MILLISECONDS_IN_SECOND) * 100;
        x86_64_lapic_timer_start(UINT32_MAX);
        time_t target = hpet_current_time() + timeout;
        while(hpet_current_time() < target) cpu_relax();
        return ((uint64_t) UINT32_MAX - x86_64_lapic_timer_read()) * (TIME_NANOSECONDS_IN_SECOND / timeout);
    }
    ASSERT_UNREACHABLE();
}

// TODO: cpuid for TSC freq if available :)
static time_frequency_t calibrate_tsc() {
    if(!g_hpet_initialized) {
        for(size_t sample_count = 8;; sample_count *= 2) {
            x86_64_pit_set_reload(0xFFF0);
            uint16_t start_count = x86_64_pit_count();
            uint64_t tsc_target = __rdtsc() + sample_count;
            while(__rdtsc() < tsc_target) cpu_relax();
            uint64_t delta = start_count - x86_64_pit_count();

            if(delta < 0x4000) continue;

            return (sample_count / MATH_MAX(1lu, delta)) * X86_64_PIT_BASE_FREQ;
        }
    } else {
        time_t timeout = (TIME_NANOSECONDS_IN_SECOND / TIME_MILLISECONDS_IN_SECOND) * 100;
        uint64_t tsc_start = __rdtsc();
        time_t target = hpet_current_time() + timeout;
        while(hpet_current_time() < target) cpu_relax();
        return (__rdtsc() - tsc_start) * (TIME_NANOSECONDS_IN_SECOND / timeout);
    }
    ASSERT_UNREACHABLE();
}


static void initialize_cpu_local(x86_64_cpu_t *cpu, size_t seqid) {
    cpu->self = cpu;

    cpu->current_thread = nullptr;
    cpu->lapic_timer_frequency = 0;
    cpu->tsc_timer_frequency = 0;
    cpu->tss = nullptr;

    cpu->lapic_id = 0;
    cpu->sequential_id = seqid;

    cpu->common.dw_items = LIST_INIT;
    cpu->common.sched = (sched_t) {
        .lock = SPINLOCK_INIT,
        .thread_queue = LIST_INIT,
        .status = { .preempt_counter = 0, .yield_immediately = false },
    };

    cpu->common.flags.threaded = false;
    cpu->common.flags.in_interrupt_hard = false;
    cpu->common.flags.in_interrupt_soft = false;
    cpu->common.flags.deferred_work_status = 0;
}

static void initialize_cpu() {
    x86_64_gdt_init();
    x86_64_interrupt_load_idt();
    pat_init();

    uint64_t cr4 = x86_64_cr4_read();
    cr4 |= 1 << 7; /* CR4.PGE */
    x86_64_cr4_write(cr4);
}

[[gnu::no_instrument_function]] [[noreturn]] static void init_ap() {
    x86_64_cpu_t *cpu = &g_x86_64_cpus[g_init_ap_cpu_id];
    x86_64_msr_write(X86_64_MSR_GS_BASE, (uint64_t) cpu);
    event_init_cpu_local();

    log(LOG_LEVEL_INFO, "INIT", "Initializing AP %lu", g_init_ap_cpu_id);

    init_stage(INIT_STAGE_BOOT, true);
    initialize_cpu();
    init_stage(INIT_STAGE_EARLY, true);

    ptm_load_address_space(g_vm_global_address_space);

    init_stage(INIT_STAGE_BEFORE_MAIN, true);
    cpu->lapic_id = x86_64_lapic_id();
    init_stage(INIT_STAGE_MAIN, true);

    init_stage(INIT_STAGE_BEFORE_DEV, true);
    init_stage(INIT_STAGE_DEV, true);

    init_stage(INIT_STAGE_LATE, true);

    log(LOG_LEVEL_DEBUG, "INIT", "AP %lu (Lapic ID: %i) init exit", g_init_ap_cpu_id, x86_64_lapic_id());
    __atomic_add_fetch(&g_init_ap_finished, true, __ATOMIC_SEQ_CST);

    sched_handoff_cpu();
    ASSERT_UNREACHABLE();
}

void init_bsp_local(size_t seqid) {
    initialize_cpu_local(&g_early_bsp, seqid);
    x86_64_msr_write(X86_64_MSR_GS_BASE, (uintptr_t) &g_early_bsp);
}

void init_bsp() {
    initialize_cpu();
}

void init_cpu_locals(tartarus_boot_info_t *boot_info) {
    log(LOG_LEVEL_DEBUG, "INIT", "Setting up proper CPU locals (%lu locals)", g_cpu_count);
    g_x86_64_cpus = heap_alloc(sizeof(x86_64_cpu_t) * g_cpu_count);
    memclear(g_x86_64_cpus, sizeof(x86_64_cpu_t) * g_cpu_count);

    for(size_t i = 0; i < boot_info->cpu_count; i++) {
        if(boot_info->cpus[i].init_state == TARTARUS_CPU_STATE_FAIL) continue;

        if(i != boot_info->bsp_index) {
            initialize_cpu_local(&g_x86_64_cpus[boot_info->cpus[i].sequential_id], boot_info->cpus[i].sequential_id);
        } else {
            log(LOG_LEVEL_DEBUG, "INIT", "Initialized BSP proper (local %lu)", boot_info->cpus[i].sequential_id);
            x86_64_cpu_t *cpu = &g_x86_64_cpus[boot_info->cpus[i].sequential_id];
            memcpy(cpu, &g_early_bsp, sizeof(x86_64_cpu_t));
            cpu->self = cpu;
            cpu->lapic_id = x86_64_lapic_id();
            x86_64_msr_write(X86_64_MSR_GS_BASE, (uint64_t) cpu);
        }
    }
}

void init_smp(tartarus_boot_info_t *boot_info) {
    for(size_t i = 0; i < boot_info->cpu_count; i++) {
        if(boot_info->cpus[i].init_state == TARTARUS_CPU_STATE_FAIL) continue;

        if(i == boot_info->bsp_index) continue;

        init_reset_ap();

        g_init_ap_cpu_id = boot_info->cpus[i].sequential_id;
        g_init_ap_finished = false;

        __atomic_store_n(boot_info->cpus[i].wake_on_write, (uintptr_t) init_ap, __ATOMIC_RELEASE);
        while(!g_init_ap_finished);
    }

    log(LOG_LEVEL_DEBUG, "INIT", "SMP init done (%lu/%lu cpus initialized)", g_cpu_count, g_cpu_count);
    log(LOG_LEVEL_DEBUG, "INIT", "BSP seqid is %lu", X86_64_CPU_CURRENT_READ(sequential_id));
}

static void cpuinfo() {
    char brand1[12];
    x86_64_cpuid_register(0, X86_64_CPUID_REGISTER_EBX, (uint32_t *) &brand1);
    x86_64_cpuid_register(0, X86_64_CPUID_REGISTER_EDX, (uint32_t *) &brand1[4]);
    x86_64_cpuid_register(0, X86_64_CPUID_REGISTER_ECX, (uint32_t *) &brand1[8]);

    char brand2[48];
    for(size_t i = 0; i < 3; i++) {
        x86_64_cpuid_register(0x80000002 + i, X86_64_CPUID_REGISTER_EAX, (uint32_t *) &brand2[i * 16]);
        x86_64_cpuid_register(0x80000002 + i, X86_64_CPUID_REGISTER_EBX, (uint32_t *) &brand2[i * 16 + 4]);
        x86_64_cpuid_register(0x80000002 + i, X86_64_CPUID_REGISTER_ECX, (uint32_t *) &brand2[i * 16 + 8]);
        x86_64_cpuid_register(0x80000002 + i, X86_64_CPUID_REGISTER_EDX, (uint32_t *) &brand2[i * 16 + 12]);
    }
    log(LOG_LEVEL_INFO, "INIT", "Running on %.*s (%.*s)", 12, brand1, 48, brand2);
}

INIT_TARGET(cpuinfo, INIT_STAGE_EARLY, cpuinfo);

static void arch_asserts() {
    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_MSR));
}

INIT_TARGET(arch_asserts, INIT_STAGE_EARLY, arch_asserts);

static void setup_tss() {
    x86_64_tss_t *tss = heap_alloc(sizeof(x86_64_tss_t));
    memclear(tss, sizeof(x86_64_tss_t));
    tss->iomap_base = sizeof(x86_64_tss_t);

    x86_64_tss_set_ist(tss, 0, HHDM(PAGE_PADDR(PAGE_FROM_BLOCK(pmm_alloc_page(PMM_FLAG_NONE))) + PAGE_GRANULARITY));
    x86_64_tss_set_ist(tss, 1, HHDM(PAGE_PADDR(PAGE_FROM_BLOCK(pmm_alloc_page(PMM_FLAG_NONE))) + PAGE_GRANULARITY));
    x86_64_interrupt_set_ist(2, 1); // Non-maskable
    x86_64_interrupt_set_ist(18, 2); // Machine check

    X86_64_CPU_CURRENT_WRITE(tss, tss);

    x86_64_gdt_load_tss(tss);
}

INIT_TARGET_PERCORE(tss, INIT_STAGE_BEFORE_MAIN, setup_tss);

static void external_interrupts() {
    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_APIC));
    // ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_ARAT)); // TODO: fails

    x86_64_pic8259_remap();
    x86_64_pic8259_disable();
    x86_64_lapic_init();
    g_x86_64_interrupt_irq_eoi = x86_64_lapic_eoi;
}

INIT_TARGET(external_interrupts, INIT_STAGE_BEFORE_MAIN, external_interrupts);
INIT_TARGET_PERCORE(lapic, INIT_STAGE_BEFORE_MAIN, x86_64_lapic_init_cpu, "external_interrupts");


static void initialize_ioapic() {
    uacpi_table madt;
    uacpi_status ret = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &madt);
    if(uacpi_likely_success(ret)) {
        x86_64_ioapic_init((struct acpi_madt *) madt.hdr);
        uacpi_table_unref(&madt);
    }
}

INIT_TARGET(ioapic, INIT_STAGE_BEFORE_DEV, initialize_ioapic, "acpi_tables")

static void initialize_timers() {
    uacpi_table hpet;
    uacpi_status ret = uacpi_table_find_by_signature(ACPI_HPET_SIGNATURE, &hpet);
    if(uacpi_likely_success(ret)) {
        x86_64_hpet_init((struct acpi_hpet *) hpet.hdr);
        uacpi_table_unref(&hpet);
        g_hpet_initialized = true;
    }

    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_TSC));
    // ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_TSC_INVARIANT)); // TODO: doesnt work on tcg

    X86_64_CPU_CURRENT_WRITE(lapic_timer_frequency, calibrate_lapic_timer());
    X86_64_CPU_CURRENT_WRITE(tsc_timer_frequency, calibrate_tsc());

    log(LOG_LEVEL_DEBUG, "INIT", "CPU[%lu] Local Apic Timer calibrated, freq: %lu", X86_64_CPU_CURRENT_READ(sequential_id), X86_64_CPU_CURRENT_READ(lapic_timer_frequency));
    log(LOG_LEVEL_DEBUG, "INIT", "CPU[%lu] TSC calibrated, freq: %lu", X86_64_CPU_CURRENT_READ(sequential_id), X86_64_CPU_CURRENT_READ(tsc_timer_frequency));
}

INIT_TARGET_PERCORE(timers, INIT_STAGE_BEFORE_DEV, initialize_timers, "acpi_tables");

static void setup_init_program() {
    log(LOG_LEVEL_DEBUG, "INIT", "loading /usr/bin/init");
    vm_address_space_t *as = ptm_address_space_create();

    vfs_node_t *init_exec;
    vfs_result_t res = vfs_lookup(&VFS_ABSOLUTE_PATH("/usr/bin/init"), &init_exec);
    if(res != VFS_RESULT_OK) panic("INIT", "failed to lookup init executable (%i)", res);
    if(init_exec == nullptr) panic("INIT", "no init executable found");

    elf_file_t *init_elf;
    elf_result_t elf_res = elf_read(init_exec, &init_elf);
    if(elf_res != ELF_RESULT_OK) panic("INIT", "could not read init (%i)", elf_res);

    elf_res = elf_load(init_elf, as);
    if(elf_res != ELF_RESULT_OK) panic("INIT", "could not load init (%i)", elf_res);

    uintptr_t entry;
    char *interpreter;
    elf_res = elf_lookup_interpreter(init_elf, &interpreter);
    switch(elf_res) {
        case ELF_RESULT_OK:
            log(LOG_LEVEL_DEBUG, "INIT", "found init interpreter `%s`", interpreter);

            vfs_node_t *interpreter_exec;
            res = vfs_lookup(&VFS_ABSOLUTE_PATH(interpreter), &interpreter_exec);
            if(res != VFS_RESULT_OK) panic("INIT", "failed to lookup interpreter for init (%i)", res);
            if(interpreter_exec == nullptr) panic("INIT", "no interpreter found for init executable");

            elf_file_t *interpreter_elf;
            elf_res = elf_read(interpreter_exec, &interpreter_elf);
            if(elf_res != ELF_RESULT_OK) panic("INIT", "could not read interpreter for init (%i)", elf_res);

            elf_res = elf_load(interpreter_elf, as);
            if(elf_res != ELF_RESULT_OK) panic("INIT", "could not load interpreter for init (%i)", elf_res);

            entry = interpreter_elf->entry;
            heap_free(interpreter_elf, sizeof(elf_file_t));
            break;
        case ELF_RESULT_ERR_NOT_FOUND: entry = init_elf->entry; break;
        default:                       panic("INIT", "elf interpreter lookup failed (%i)", elf_res);
    }

    x86_64_auxv_t auxv = { .entry = init_elf->entry };

    uintptr_t phdr_addr;
    elf_res = elf_lookup_phdr_address(init_elf, &phdr_addr);
    if(elf_res == ELF_RESULT_OK) {
        auxv.phdr = phdr_addr;
        auxv.phnum = init_elf->program_headers.count;
        auxv.phent = init_elf->program_headers.entry_size;
    }

    heap_free(init_elf, sizeof(elf_file_t));

    log(LOG_LEVEL_DEBUG, "INIT", "auxv { entry: %#lx; phdr: %#lx; phent: %#lx; phnum: %#lx; }", auxv.entry, auxv.phdr, auxv.phent, auxv.phnum);

    char *argv[] = { "/usr/bin/init", nullptr };
    char *envp[] = { nullptr };

    process_t *proc = process_create(as);
    uintptr_t thread_stack = x86_64_sysv_stack_setup(proc->address_space, PAGE_GRANULARITY * 8, argv, envp, &auxv);
    thread_t *thread = sched_thread_create_user(proc, entry, thread_stack);

    log(LOG_LEVEL_DEBUG, "INIT", "init thread (tid: %lu) >> entry: %#lx, stack: %#lx", thread->id, entry, thread_stack);

    sched_thread_schedule(thread);
}

INIT_TARGET(init_program, INIT_STAGE_LATE, setup_init_program);
