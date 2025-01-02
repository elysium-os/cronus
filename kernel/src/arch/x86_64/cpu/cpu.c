#include "arch/cpu.h"

#include "common/assert.h"

#include "arch/x86_64/cpu/cpu.h"

cpu_t *arch_cpu_current() {
    return &X86_64_CPU_LOCAL_MEMBER(self)->common;
}

void arch_cpu_relax() {
    __builtin_ia32_pause();
}

[[noreturn]] void arch_cpu_halt() {
    for(;;) asm volatile("hlt");
    __builtin_unreachable();
}
