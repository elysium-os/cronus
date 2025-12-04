#pragma once

#include "sys/time.h"
#include "x86_64/cpu/tss.h"
#include "x86_64/thread.h"

#include <stddef.h>
#include <stdint.h>

typedef struct arch_cpu_data {
    uint32_t lapic_id;

    time_frequency_t lapic_timer_frequency;
    time_frequency_t tsc_timer_frequency;

    x86_64_tss_t *tss;

    x86_64_thread_t *current_thread; // TODO: move (when we fix the thread pattern)
} arch_cpu_data_t;
