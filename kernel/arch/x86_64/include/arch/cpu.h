#pragma once

#include "common/assert.h"
#include "x86_64/cpu/cpu.h"

#define CPU_CURRENT_READ(FIELD) X86_64_CPU_CURRENT_READ(common.FIELD)
#define CPU_CURRENT_WRITE(FIELD, VALUE) X86_64_CPU_CURRENT_WRITE(common.FIELD, (VALUE))

#define CPU_CURRENT_EXCHANGE(FIELD, VALUE) X86_64_CPU_CURRENT_EXCHANGE(common.FIELD, (VALUE))

#define CPU_CURRENT_INC(FIELD) X86_64_CPU_CURRENT_INC(common.FIELD)
#define CPU_CURRENT_DEC(FIELD) X86_64_CPU_CURRENT_DEC(common.FIELD)

#define CPU_CURRENT_PTR() (&X86_64_CPU_CURRENT_PTR()->common)
#define CPU_CURRENT_THREAD() (&X86_64_CPU_CURRENT_READ(current_thread)->common)

/// Get cpu id. Guaranteed to be sequential.
static inline size_t cpu_id() {
    return X86_64_CPU_CURRENT_READ(sequential_id);
}

/// "Relax" the CPU.
static inline void cpu_relax() {
    __builtin_ia32_pause();
}

/// Halt the CPU.
[[noreturn]] static inline void cpu_halt() {
    while(true) {
        __builtin_ia32_pause();
        asm volatile("hlt");
    }
    ASSERT_UNREACHABLE();
}
