#pragma once

#include "common/lock/spinlock.h"
#include "lib/container.h"
#include "sys/cpu.h"

#include "arch/x86_64/cpu/tss.h"
#include "arch/x86_64/thread.h"

#include <stddef.h>
#include <stdint.h>

#define X86_64_CPU(CPU) (CONTAINER_OF((CPU), x86_64_cpu_t, common))

#define X86_64_CPU_LOCAL_MEMBER(MEMBER)                                                         \
    ({                                                                                          \
        typeof((x86_64_cpu_t) {}.MEMBER) _value;                                                \
        asm volatile("mov %%gs:(%1), %0" : "=r"(_value) : "r"(offsetof(x86_64_cpu_t, MEMBER))); \
        _value;                                                                                 \
    })

#define X86_64_CPU_LOCAL_MEMBER_SET(MEMBER, VALUE) asm volatile("mov %0, %%gs:(%1)" : : "r"(VALUE), "r"((offsetof(x86_64_cpu_t, MEMBER))))

typedef struct x86_64_cpu {
    struct x86_64_cpu *self;

    size_t sequential_id;

    uint32_t lapic_id;
    uint64_t lapic_timer_frequency;

    uintptr_t tlb_shootdown_cr3;
    uintptr_t tlb_shootdown_addr;
    spinlock_t tlb_shootdown_check; /* used as a primitive spinlock */
    spinlock_t tlb_shootdown_lock;

    x86_64_tss_t *tss;

    x86_64_thread_t *current_thread;

    cpu_t common;
} x86_64_cpu_t;

extern size_t g_x86_64_cpu_count;
extern x86_64_cpu_t *g_x86_64_cpus;
