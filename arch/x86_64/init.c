#include "sys/init.h"

#include "abi/sysv/elf.h"
#include "arch/cpu.h"
#include "arch/page.h"
#include "arch/ptm.h"
#include "arch/sched.h"
#include "common/assert.h"
#include "common/log.h"
#include "fs/vfs.h"
#include "init.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "memory/heap.h"
#include "memory/hhdm.h"
#include "memory/page.h"
#include "memory/vm.h"
#include "sched/sched.h"
#include "sys/init.h"
#include "sys/time.h"
#include "x86_64/abi/sysv/auxv.h"
#include "x86_64/abi/sysv/sysv.h"
#include "x86_64/cpu/cpuid.h"
#include "x86_64/cpu/lapic.h"
#include "x86_64/cpu/msr.h"
#include "x86_64/dev/hpet.h"
#include "x86_64/dev/pit.h"

#include <stddef.h>
#include <stdint.h>
#include <tartarus.h>
#include <uacpi/tables.h>
#include <uacpi/uacpi.h>

uintptr_t g_hhdm_offset;
size_t g_hhdm_size;

page_t *g_page_db;
size_t g_page_db_size;

static volatile bool g_init_ap_finished = false;
static uint64_t g_init_ap_cpu_id = 0;

static cpu_t g_early_bsp;

static bool g_hpet_initialized = false;

static time_frequency_t calibrate_lapic_timer() {
    uint32_t nominal_freq = 0;
    if(!x86_64_cpuid_register(0x15, X86_64_CPUID_REGISTER_ECX, &nominal_freq) && nominal_freq != 0) { return (time_frequency_t) nominal_freq; }

    x86_64_lapic_timer_setup(X86_64_LAPIC_TIMER_TYPE_ONESHOT, true, 0xFF, X86_64_LAPIC_TIMER_DIVISOR_16);
    if(!g_hpet_initialized) {
        for(size_t sample_count = 8;; sample_count *= 2) {
            x86_64_pit_set_reload(0xFFF0);
            uint16_t start_count = x86_64_pit_count();
            x86_64_lapic_timer_start(sample_count);
            while(x86_64_lapic_timer_read() != 0) arch_cpu_relax();
            uint64_t delta = start_count - x86_64_pit_count();

            if(delta < 0x4000) continue;

            x86_64_lapic_timer_stop();
            return (sample_count / MATH_MAX(1lu, delta)) * X86_64_PIT_BASE_FREQ;
        }
    } else {
        time_t timeout = (TIME_NANOSECONDS_IN_SECOND / TIME_MILLISECONDS_IN_SECOND) * 100;
        x86_64_lapic_timer_start(UINT32_MAX);
        time_t target = hpet_current_time() + timeout;
        while(hpet_current_time() < target) arch_cpu_relax();
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
            while(__rdtsc() < tsc_target) arch_cpu_relax();
            uint64_t delta = start_count - x86_64_pit_count();

            if(delta < 0x4000) continue;

            return (sample_count / MATH_MAX(1lu, delta)) * X86_64_PIT_BASE_FREQ;
        }
    } else {
        time_t timeout = (TIME_NANOSECONDS_IN_SECOND / TIME_MILLISECONDS_IN_SECOND) * 100;
        uint64_t tsc_start = __rdtsc();
        time_t target = hpet_current_time() + timeout;
        while(hpet_current_time() < target) arch_cpu_relax();
        return (__rdtsc() - tsc_start) * (TIME_NANOSECONDS_IN_SECOND / timeout);
    }
    ASSERT_UNREACHABLE();
}

[[gnu::no_instrument_function]] [[noreturn]] static void init_ap() {
    cpu_t *cpu = &g_cpu_list[g_init_ap_cpu_id];
    cpu->self = cpu;
    cpu->sequential_id = g_init_ap_cpu_id;
    x86_64_msr_write(X86_64_MSR_GS_BASE, (uint64_t) cpu);

    log(LOG_LEVEL_INFO, "INIT", "Initializing AP %lu", g_init_ap_cpu_id);

    init_run(true);

    log(LOG_LEVEL_DEBUG, "INIT", "AP %lu (Lapic ID: %i) init exit", g_init_ap_cpu_id, x86_64_lapic_id());
    __atomic_add_fetch(&g_init_ap_finished, true, __ATOMIC_SEQ_CST);

    arch_sched_handoff_cpu();
    ASSERT_UNREACHABLE();
}

void arch_init_smp(tartarus_boot_info_t *boot_info) {
    for(size_t i = 0; i < boot_info->cpu_count; i++) {
        if(boot_info->cpus[i].init_state == TARTARUS_CPU_STATE_FAIL) continue;

        if(i == boot_info->bsp_index) continue;

        g_init_ap_cpu_id = boot_info->cpus[i].sequential_id;
        g_init_ap_finished = false;

        __atomic_store_n(boot_info->cpus[i].wake_on_write, (uintptr_t) init_ap, __ATOMIC_RELEASE);
        while(!g_init_ap_finished);
    }

    log(LOG_LEVEL_DEBUG, "INIT", "SMP init done (%lu/%lu cpus initialized)", g_cpu_count, g_cpu_count);
    log(LOG_LEVEL_DEBUG, "INIT", "BSP seqid is %lu", ARCH_CPU_CURRENT_READ(sequential_id));
}

extern log_sink_t g_qemu_debug_sink;
void arch_init_bsp() {
    g_early_bsp.self = &g_early_bsp;
    x86_64_msr_write(X86_64_MSR_GS_BASE, (uintptr_t) &g_early_bsp);

    log_sink_add(&g_qemu_debug_sink);
}

INIT_TARGET_PERCORE(timers, INIT_PROVIDES("timer"), INIT_DEPS("acpi_tables", "log")) {
    uacpi_table hpet;
    uacpi_status ret = uacpi_table_find_by_signature(ACPI_HPET_SIGNATURE, &hpet);
    if(uacpi_likely_success(ret)) {
        x86_64_hpet_init((struct acpi_hpet *) hpet.hdr);
        uacpi_table_unref(&hpet);
        g_hpet_initialized = true;
    }

    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_TSC));
    // ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_TSC_INVARIANT)); // TODO: doesnt work on tcg

    ARCH_CPU_CURRENT_WRITE(arch.lapic_timer_frequency, calibrate_lapic_timer());
    ARCH_CPU_CURRENT_WRITE(arch.tsc_timer_frequency, calibrate_tsc());

    log(LOG_LEVEL_DEBUG, "INIT", "CPU[%lu] Local Apic Timer calibrated, freq: %lu", ARCH_CPU_CURRENT_READ(sequential_id), ARCH_CPU_CURRENT_READ(arch.lapic_timer_frequency));
    log(LOG_LEVEL_DEBUG, "INIT", "CPU[%lu] TSC calibrated, freq: %lu", ARCH_CPU_CURRENT_READ(sequential_id), ARCH_CPU_CURRENT_READ(arch.tsc_timer_frequency));
}

INIT_TARGET_PERCORE(misc_cpu_local, INIT_PROVIDES("cpu_local"), INIT_DEPS()) {
    // TODO: move into proper targets
    ARCH_CPU_CURRENT_WRITE(arch.current_thread, (x86_64_thread_t *) nullptr);
    ARCH_CPU_CURRENT_WRITE(arch.tss, (x86_64_tss_t *) nullptr);
    ARCH_CPU_CURRENT_WRITE(arch.lapic_timer_frequency, (size_t) 0);
    ARCH_CPU_CURRENT_WRITE(arch.tsc_timer_frequency, (size_t) 0);
}

INIT_TARGET(bsp_late_cpu_local, INIT_PROVIDES("late_cpu_local"), INIT_DEPS("heap", "log", "cpu_local")) {
    log(LOG_LEVEL_DEBUG, "INIT", "Setting up proper CPU locals (%lu locals)", g_cpu_count);
    g_cpu_list = heap_alloc(sizeof(cpu_t) * g_cpu_count);
    mem_clear(g_cpu_list, sizeof(cpu_t) * g_cpu_count);

    cpu_t *cpu = &g_cpu_list[g_init_boot_info->cpus[g_init_boot_info->bsp_index].sequential_id];
    mem_copy(cpu, &g_early_bsp, sizeof(cpu_t));
    cpu->self = cpu;
    x86_64_msr_write(X86_64_MSR_GS_BASE, (uint64_t) cpu);

    log(LOG_LEVEL_DEBUG, "INIT", "Initialized BSP proper (local %lu)", cpu->sequential_id);
}

INIT_TARGET(cpu_info, INIT_PROVIDES(), INIT_DEPS("log")) {
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
};

INIT_TARGET(arch_asserts, INIT_PROVIDES("arch"), INIT_DEPS("log")) {
    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_MSR));
}

INIT_TARGET(init_program, INIT_PROVIDES("init"), INIT_DEPS("heap", "vm", "pmm", "ptm", "sched", "vfs", "log")) {
    log(LOG_LEVEL_DEBUG, "INIT", "loading /usr/bin/init");
    vm_address_space_t *as = arch_ptm_address_space_create();

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
    uintptr_t thread_stack = x86_64_sysv_stack_setup(proc->address_space, ARCH_PAGE_GRANULARITY * 8, argv, envp, &auxv);
    thread_t *thread = arch_sched_thread_create_user(proc, entry, thread_stack);

    log(LOG_LEVEL_DEBUG, "INIT", "init thread (tid: %lu) >> entry: %#lx, stack: %#lx", thread->id, entry, thread_stack);

    sched_thread_schedule(thread);
}
