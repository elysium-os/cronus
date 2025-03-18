#include "arch/cpu.h"

#include "arch/x86_64/cpu/cpu.h"

cpu_t *arch_cpu_current() {
    return &X86_64_CPU_LOCAL_MEMBER(self)->common;
}

size_t arch_cpu_count() {
    return g_x86_64_cpu_count;
}

size_t arch_cpu_id() {
    return X86_64_CPU_LOCAL_MEMBER(sequential_id);
}

void arch_cpu_relax() {
    __builtin_ia32_pause();
}

[[noreturn]] void arch_cpu_halt() {
    while(true) {
        __builtin_ia32_pause();
        asm volatile("hlt");
    }
    __builtin_unreachable();
}
