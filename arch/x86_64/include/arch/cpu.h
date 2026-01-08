#pragma once

#include "../../../../include/arch/cpu.h"
#include "common/assert.h"
#include "sys/cpu.h"

#include <stddef.h>

#define ARCH_CPU_CURRENT_READ(FIELD)                                                  \
    ({                                                                                \
        typeof(((cpu_t *) nullptr)->FIELD) value;                                     \
        asm volatile("mov %%gs:%c1, %0" : "=r"(value) : "i"(offsetof(cpu_t, FIELD))); \
        value;                                                                        \
    })

#define ARCH_CPU_CURRENT_WRITE(FIELD, VALUE)                                                                                                             \
    ({                                                                                                                                                   \
        static_assert(__builtin_types_compatible_p(typeof(((cpu_t *) nullptr)->FIELD), typeof(VALUE)), "member type and value type are not compatible"); \
        typeof(((cpu_t *) nullptr)->FIELD) value = (VALUE);                                                                                              \
        asm volatile("mov %0, %%gs:%c1" : : "r"(value), "i"(offsetof(cpu_t, FIELD)));                                                                    \
    })

#define ARCH_CPU_CURRENT_EXCHANGE(FIELD, VALUE)                                                                                                          \
    ({                                                                                                                                                   \
        static_assert(__builtin_types_compatible_p(typeof(((cpu_t *) nullptr)->FIELD), typeof(VALUE)), "member type and value type are not compatible"); \
        typeof(((cpu_t *) nullptr)->FIELD) value = (VALUE);                                                                                              \
        asm volatile("xchg %0, %%gs:%c1" : "+r"(value) : "i"(offsetof(cpu_t, FIELD)) : "memory");                                                        \
        value;                                                                                                                                           \
    })

#define ARCH_CPU_CURRENT_INC_64(FIELD) ({ asm volatile("incq %%gs:%c0" : : "i"(offsetof(cpu_t, FIELD)) : "memory"); })
#define ARCH_CPU_CURRENT_INC_32(FIELD) ({ asm volatile("incl %%gs:%c0" : : "i"(offsetof(cpu_t, FIELD)) : "memory"); })
#define ARCH_CPU_CURRENT_INC_16(FIELD) ({ asm volatile("incw %%gs:%c0" : : "i"(offsetof(cpu_t, FIELD)) : "memory"); })
#define ARCH_CPU_CURRENT_INC_8(FIELD) ({ asm volatile("incb %%gs:%c0" : : "i"(offsetof(cpu_t, FIELD)) : "memory"); })

#define ARCH_CPU_CURRENT_INC(FIELD)               \
    _Generic(                                     \
        (((cpu_t *) nullptr)->FIELD),             \
        uint64_t: ARCH_CPU_CURRENT_INC_64(FIELD), \
        int64_t: ARCH_CPU_CURRENT_INC_64(FIELD),  \
        uint32_t: ARCH_CPU_CURRENT_INC_32(FIELD), \
        int32_t: ARCH_CPU_CURRENT_INC_32(FIELD),  \
        uint16_t: ARCH_CPU_CURRENT_INC_16(FIELD), \
        int16_t: ARCH_CPU_CURRENT_INC_16(FIELD),  \
        uint8_t: ARCH_CPU_CURRENT_INC_8(FIELD),   \
        int8_t: ARCH_CPU_CURRENT_INC_8(FIELD)     \
    )

#define ARCH_CPU_CURRENT_DEC_64(FIELD) ({ asm volatile("decq %%gs:%c0" : : "i"(offsetof(cpu_t, FIELD)) : "memory"); })
#define ARCH_CPU_CURRENT_DEC_32(FIELD) ({ asm volatile("decl %%gs:%c0" : : "i"(offsetof(cpu_t, FIELD)) : "memory"); })
#define ARCH_CPU_CURRENT_DEC_16(FIELD) ({ asm volatile("decw %%gs:%c0" : : "i"(offsetof(cpu_t, FIELD)) : "memory"); })
#define ARCH_CPU_CURRENT_DEC_8(FIELD) ({ asm volatile("decb %%gs:%c0" : : "i"(offsetof(cpu_t, FIELD)) : "memory"); })

#define ARCH_CPU_CURRENT_DEC(FIELD)               \
    _Generic(                                     \
        (((cpu_t *) nullptr)->FIELD),             \
        uint64_t: ARCH_CPU_CURRENT_DEC_64(FIELD), \
        int64_t: ARCH_CPU_CURRENT_DEC_64(FIELD),  \
        uint32_t: ARCH_CPU_CURRENT_DEC_32(FIELD), \
        int32_t: ARCH_CPU_CURRENT_DEC_32(FIELD),  \
        uint16_t: ARCH_CPU_CURRENT_DEC_16(FIELD), \
        int16_t: ARCH_CPU_CURRENT_DEC_16(FIELD),  \
        uint8_t: ARCH_CPU_CURRENT_DEC_8(FIELD),   \
        int8_t: ARCH_CPU_CURRENT_DEC_8(FIELD)     \
    )

#define ARCH_CPU_CURRENT_PTR() ARCH_CPU_CURRENT_READ(self)
#define ARCH_CPU_CURRENT_THREAD() ARCH_CPU_CURRENT_READ(arch.current_thread)

/// "Relax" the CPU.
static inline void arch_cpu_relax() {
    __builtin_ia32_pause();
}

/// Halt the CPU.
[[noreturn]] static inline void arch_cpu_halt() {
    while(true) {
        __builtin_ia32_pause();
        asm volatile("hlt");
    }
    ASSERT_UNREACHABLE();
}
