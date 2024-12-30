#pragma once

#include "common/spinlock.h"
#include "lib/container.h"
#include "sys/cpu.h"

#include "arch/x86_64/sys/tss.h"

#include <stddef.h>
#include <stdint.h>

#define X86_64_CPU(CPU) (CONTAINER_OF((CPU), x86_64_cpu_t, common))

typedef struct {
    uint32_t lapic_id;
    uint64_t lapic_timer_frequency;

    uintptr_t tlb_shootdown_cr3;
    uintptr_t tlb_shootdown_addr;
    spinlock_t tlb_shootdown_check;
    spinlock_t tlb_shootdown_lock;

    x86_64_tss_t *tss;

    cpu_t common;
} x86_64_cpu_t;

extern volatile size_t g_x86_64_cpu_count;
extern x86_64_cpu_t *g_x86_64_cpus;
