#pragma once

#include "lib/container.h"
#include "sys/cpu.h"
#include "sys/time.h"
#include "x86_64/cpu/tss.h"
#include "x86_64/thread.h"

#include <stddef.h>
#include <stdint.h>

#define X86_64_CPU(CPU) (CONTAINER_OF((CPU), x86_64_cpu_t, common))

#define X86_64_CPU_CURRENT_READ(FIELD)                                                       \
    ({                                                                                       \
        typeof(((x86_64_cpu_t *) nullptr)->FIELD) value;                                     \
        asm volatile("mov %%gs:%c1, %0" : "=r"(value) : "i"(offsetof(x86_64_cpu_t, FIELD))); \
        value;                                                                               \
    })

#define X86_64_CPU_CURRENT_WRITE(FIELD, VALUE)                                                                                                                  \
    ({                                                                                                                                                          \
        static_assert(__builtin_types_compatible_p(typeof(((x86_64_cpu_t *) nullptr)->FIELD), typeof(VALUE)), "member type and value type are not compatible"); \
        typeof(((x86_64_cpu_t *) nullptr)->FIELD) value = (VALUE);                                                                                              \
        asm volatile("mov %0, %%gs:%c1" : : "r"(value), "i"(offsetof(x86_64_cpu_t, FIELD)));                                                                    \
    })

#define X86_64_CPU_CURRENT_EXCHANGE(FIELD, VALUE)                                                                                                               \
    ({                                                                                                                                                          \
        static_assert(__builtin_types_compatible_p(typeof(((x86_64_cpu_t *) nullptr)->FIELD), typeof(VALUE)), "member type and value type are not compatible"); \
        typeof(((x86_64_cpu_t *) nullptr)->FIELD) value = (VALUE);                                                                                              \
        asm volatile("xchg %0, %%gs:%c1" : "+r"(value) : "i"(offsetof(x86_64_cpu_t, FIELD)) : "memory");                                                        \
        value;                                                                                                                                                  \
    })

#define X86_64_CPU_CURRENT_INC_64(FIELD) ({ asm volatile("incq %%gs:%c0" : : "i"(offsetof(x86_64_cpu_t, FIELD)) : "memory"); })
#define X86_64_CPU_CURRENT_INC_32(FIELD) ({ asm volatile("incl %%gs:%c0" : : "i"(offsetof(x86_64_cpu_t, FIELD)) : "memory"); })
#define X86_64_CPU_CURRENT_INC_16(FIELD) ({ asm volatile("incw %%gs:%c0" : : "i"(offsetof(x86_64_cpu_t, FIELD)) : "memory"); })
#define X86_64_CPU_CURRENT_INC_8(FIELD) ({ asm volatile("incb %%gs:%c0" : : "i"(offsetof(x86_64_cpu_t, FIELD)) : "memory"); })

#define X86_64_CPU_CURRENT_INC(FIELD)               \
    _Generic(                                       \
        (((x86_64_cpu_t *) nullptr)->FIELD),        \
        uint64_t: X86_64_CPU_CURRENT_INC_64(FIELD), \
        int64_t: X86_64_CPU_CURRENT_INC_64(FIELD),  \
        uint32_t: X86_64_CPU_CURRENT_INC_32(FIELD), \
        int32_t: X86_64_CPU_CURRENT_INC_32(FIELD),  \
        uint16_t: X86_64_CPU_CURRENT_INC_16(FIELD), \
        int16_t: X86_64_CPU_CURRENT_INC_16(FIELD),  \
        uint8_t: X86_64_CPU_CURRENT_INC_8(FIELD),   \
        int8_t: X86_64_CPU_CURRENT_INC_8(FIELD)     \
    )

#define X86_64_CPU_CURRENT_DEC_64(FIELD) ({ asm volatile("decq %%gs:%c0" : : "i"(offsetof(x86_64_cpu_t, FIELD)) : "memory"); })
#define X86_64_CPU_CURRENT_DEC_32(FIELD) ({ asm volatile("decl %%gs:%c0" : : "i"(offsetof(x86_64_cpu_t, FIELD)) : "memory"); })
#define X86_64_CPU_CURRENT_DEC_16(FIELD) ({ asm volatile("decw %%gs:%c0" : : "i"(offsetof(x86_64_cpu_t, FIELD)) : "memory"); })
#define X86_64_CPU_CURRENT_DEC_8(FIELD) ({ asm volatile("decb %%gs:%c0" : : "i"(offsetof(x86_64_cpu_t, FIELD)) : "memory"); })

#define X86_64_CPU_CURRENT_DEC(FIELD)               \
    _Generic(                                       \
        (((x86_64_cpu_t *) nullptr)->FIELD),        \
        uint64_t: X86_64_CPU_CURRENT_DEC_64(FIELD), \
        int64_t: X86_64_CPU_CURRENT_DEC_64(FIELD),  \
        uint32_t: X86_64_CPU_CURRENT_DEC_32(FIELD), \
        int32_t: X86_64_CPU_CURRENT_DEC_32(FIELD),  \
        uint16_t: X86_64_CPU_CURRENT_DEC_16(FIELD), \
        int16_t: X86_64_CPU_CURRENT_DEC_16(FIELD),  \
        uint8_t: X86_64_CPU_CURRENT_DEC_8(FIELD),   \
        int8_t: X86_64_CPU_CURRENT_DEC_8(FIELD)     \
    )

#define X86_64_CPU_CURRENT_PTR() X86_64_CPU_CURRENT_READ(self)
#define X86_64_CPU_CURRENT_THREAD() X86_64_CPU_CURRENT_READ(current_thread)

typedef struct x86_64_cpu {
    struct x86_64_cpu *self;

    size_t sequential_id;

    uint32_t lapic_id;

    time_frequency_t lapic_timer_frequency;
    time_frequency_t tsc_timer_frequency;

    x86_64_tss_t *tss;

    x86_64_thread_t *current_thread;

    cpu_t common;
} x86_64_cpu_t;

static_assert(offsetof(x86_64_cpu_t, current_thread) == 48, "current_thread in x86_64_cpu_t changed. Update arch/x86_64/syscall.asm::CURRENT_THREAD_OFFSET");

extern x86_64_cpu_t *g_x86_64_cpus;
