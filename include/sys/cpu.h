#pragma once

#include "arch/cpu.h"
#include "common/attr.h"

#include <stdint.h>

#define CPU_LOCAL_PTR(VAR, TYPE)                                                                                         \
    ({                                                                                                                   \
        static_assert(__builtin_types_compatible_p(typeof(VAR), TYPE));                                                  \
        (TYPE *) (ATOMIC_LOAD(&gc_cpu_self, ATOMIC_RELAXED) + (uintptr_t) (&VAR) - (uintptr_t) ld_cpu_local_init_start); \
    })

typedef struct {
    ATTR(atomic) bool in_interrupt_hard;
    ATTR(atomic) bool in_interrupt_soft;
    ATTR(atomic) size_t deferred_work_status;
    ATTR(atomic) bool threaded;
} cpu_flags_t;

extern nullptr_t ld_cpu_local_init_start[];
extern nullptr_t ld_cpu_local_init_end[];
extern nullptr_t ld_cpu_local_bsp_start[];
extern nullptr_t ld_cpu_local_bsp_end[];

ATTR(cpu_local, atomic) uintptr_t gc_cpu_self;
ATTR(cpu_local, atomic) size_t gc_cpu_sequential_id;
ATTR(cpu_local) cpu_flags_t gc_cpu_flags;

extern size_t g_cpu_count;
