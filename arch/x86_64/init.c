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

INIT_TARGET_PERCORE(misc_cpu_local, INIT_PROVIDES("cpu_local"), INIT_DEPS()) {
    // TODO: move into proper targets
    ARCH_CPU_CURRENT_WRITE(arch.current_thread, (x86_64_thread_t *) nullptr);
    ARCH_CPU_CURRENT_WRITE(arch.tss, (x86_64_tss_t *) nullptr);
    ARCH_CPU_CURRENT_WRITE(arch.lapic_timer_frequency, (size_t) 0);
    ARCH_CPU_CURRENT_WRITE(arch.tsc_timer_frequency, (size_t) 0);
}

INIT_TARGET(bsp_late_cpu_local, INIT_PROVIDES(), INIT_DEPS("heap", "log", "cpu_local")) {
    log(LOG_LEVEL_DEBUG, "INIT", "Setting up proper CPU locals (%lu locals)", g_cpu_count);
    g_cpu_list = heap_alloc(sizeof(cpu_t) * g_cpu_count);
    mem_clear(g_cpu_list, sizeof(cpu_t) * g_cpu_count);

    cpu_t *cpu = &g_cpu_list[g_init_boot_info->cpus[g_init_boot_info->bsp_index].sequential_id];
    mem_copy(cpu, &g_early_bsp, sizeof(cpu_t));
    cpu->self = cpu;
    x86_64_msr_write(X86_64_MSR_GS_BASE, (uint64_t) cpu);

    log(LOG_LEVEL_DEBUG, "INIT", "Initialized BSP proper (local %lu)", cpu->sequential_id);
}

INIT_TARGET(init_program, INIT_PROVIDES(), INIT_DEPS("memory", "sched", "fs", "panic", "log")) {
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

INIT_TARGET(msr, INIT_PROVIDES("cpu"), INIT_DEPS("assert")) {
    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_MSR));
}
