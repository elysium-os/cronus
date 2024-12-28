#pragma once

#include "common/spinlock.h"
#include "lib/container.h"
#include "arch/x86_64/sys/tss.h"

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t lapic_id;
    uint64_t lapic_timer_frequency;

    uintptr_t tlb_shootdown_cr3;
    uintptr_t tlb_shootdown_addr;
    spinlock_t tlb_shootdown_check;
    spinlock_t tlb_shootdown_lock;

    x86_64_tss_t *tss;

} x86_64_cpu_t;

extern volatile size_t g_x86_64_cpu_count;
extern x86_64_cpu_t *g_x86_64_cpus;