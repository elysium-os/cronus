#include "arch/cpu.h"

#include "common/assert.h"
#include "x86_64/cpu/cpu.h"

cpu_t *cpu_current() {
    return &X86_64_CPU_CURRENT.self->common;
}

size_t cpu_id() {
    return X86_64_CPU_CURRENT.sequential_id;
}

void cpu_relax() {
    __builtin_ia32_pause();
}

[[noreturn]] void cpu_halt() {
    while(true) {
        __builtin_ia32_pause();
        asm volatile("hlt");
    }
    ASSERT_UNREACHABLE();
}
