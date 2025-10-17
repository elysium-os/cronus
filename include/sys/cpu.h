#pragma once

#include "lib/list.h"
#include "sched/sched.h"

#include <stdint.h>

extern size_t g_cpu_count;

typedef struct cpu {
    sched_t sched;
    rb_tree_t events;
    rb_tree_t free_events; // Events to be freed
    list_t dw_items;
    struct {
        uint32_t deferred_work_status;
        bool threaded;
        bool in_interrupt_hard;
        bool in_interrupt_soft;
    } flags;
} cpu_t;
