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
#include "sys/hook.h"
#include "sys/init.h"
#include "sys/time.h"
#include "x86_64/abi/sysv/auxv.h"
#include "x86_64/abi/sysv/sysv.h"
#include "x86_64/cpu/cpuid.h"
#include "x86_64/cpu/cr.h"
#include "x86_64/cpu/gdt.h"
#include "x86_64/cpu/lapic.h"
#include "x86_64/cpu/msr.h"
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
        time_t target = x86_64_hpet_current_time() + timeout;
        while(x86_64_hpet_current_time() < target) arch_cpu_relax();
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
        time_t target = x86_64_hpet_current_time() + timeout;
        while(x86_64_hpet_current_time() < target) arch_cpu_relax();
        return (__rdtsc() - tsc_start) * (TIME_NANOSECONDS_IN_SECOND / timeout);
    }
    ASSERT_UNREACHABLE();
}

INIT_TARGET(cpuinfo, INIT_STAGE_EARLY, INIT_SCOPE_BSP, INIT_DEPS()) {
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

INIT_TARGET(arch_asserts, INIT_STAGE_EARLY, INIT_SCOPE_BSP, INIT_DEPS()) {
    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_MSR));
}

INIT_TARGET(tss, INIT_STAGE_BEFORE_MAIN, INIT_SCOPE_ALL, INIT_DEPS()) {
    x86_64_tss_t *tss = heap_alloc(sizeof(x86_64_tss_t));
    mem_clear(tss, sizeof(x86_64_tss_t));
    tss->iomap_base = sizeof(x86_64_tss_t);

    x86_64_tss_set_ist(tss, 0, HHDM(PAGE_PADDR(PAGE_FROM_BLOCK(pmm_alloc_page(PMM_FLAG_NONE))) + ARCH_PAGE_GRANULARITY));
    x86_64_tss_set_ist(tss, 1, HHDM(PAGE_PADDR(PAGE_FROM_BLOCK(pmm_alloc_page(PMM_FLAG_NONE))) + ARCH_PAGE_GRANULARITY));
    x86_64_interrupt_set_ist(2, 1); // Non-maskable
    x86_64_interrupt_set_ist(18, 2); // Machine check

    ARCH_CPU_CURRENT_WRITE(arch.tss, tss);

    x86_64_gdt_load_tss(tss);
}

INIT_TARGET(external_interrupts, INIT_STAGE_BEFORE_MAIN, INIT_SCOPE_BSP, INIT_DEPS()) {
    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_APIC));
    // ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_ARAT)); // TODO: fails

    x86_64_pic8259_remap();
    x86_64_pic8259_disable();
    x86_64_lapic_init();
    g_x86_64_interrupt_irq_eoi = x86_64_lapic_eoi;
}

INIT_TARGET(ioapic, INIT_STAGE_BEFORE_DEV, INIT_SCOPE_BSP, INIT_DEPS("acpi_tables")) {
    uacpi_table madt;
    uacpi_status ret = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &madt);
    if(uacpi_likely_success(ret)) {
        x86_64_ioapic_init((struct acpi_madt *) madt.hdr);
        uacpi_table_unref(&madt);
    }
}

INIT_TARGET(timers, INIT_STAGE_BEFORE_DEV, INIT_SCOPE_ALL, INIT_DEPS("acpi_tables")) {
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

INIT_TARGET(init_program, INIT_STAGE_LATE, INIT_SCOPE_BSP, INIT_DEPS()) {
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

HOOK(init_cpu_local) {
    ARCH_CPU_CURRENT_PTR()->flags.yield_immediately = false;
    ARCH_CPU_CURRENT_PTR()->flags.threaded = false;
    ARCH_CPU_CURRENT_PTR()->flags.in_interrupt_hard = false;
    ARCH_CPU_CURRENT_PTR()->flags.in_interrupt_soft = false;
    ARCH_CPU_CURRENT_PTR()->flags.deferred_work_status = 0;
    ARCH_CPU_CURRENT_PTR()->flags.preempt_counter = 0;
}

HOOK(init_cpu_local) { // TODO: these are purely defaults, should be moved at the very least, probably not really needed
    ARCH_CPU_CURRENT_PTR()->arch.current_thread = nullptr;
    ARCH_CPU_CURRENT_PTR()->arch.lapic_timer_frequency = 0;
    ARCH_CPU_CURRENT_PTR()->arch.tsc_timer_frequency = 0;
    ARCH_CPU_CURRENT_PTR()->arch.tss = nullptr;
    ARCH_CPU_CURRENT_PTR()->arch.lapic_id = 0;
}
