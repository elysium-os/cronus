#include "arch/cpu.h"

void arch_cpu_relax() {
    __builtin_ia32_pause();
}

[[noreturn]] void arch_cpu_halt() {
    asm volatile("cli");
    for(;;) asm volatile("hlt");
    __builtin_unreachable();
}
