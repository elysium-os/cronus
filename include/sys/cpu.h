#pragma once

#include "arch/cpu_data.h"
#include "lib/list.h"

#include <stdint.h>

typedef struct cpu {
    struct cpu *self;

    size_t sequential_id;

    rb_tree_t events;
    rb_tree_t free_events; // Events to be freed
    list_t dw_items;
    struct {
        uint32_t deferred_work_status;
        uint32_t preempt_counter;
        bool yield_immediately;
        bool threaded;
        bool in_interrupt_hard;
        bool in_interrupt_soft;
    } flags;
    arch_cpu_data_t arch;
} cpu_t;

extern size_t g_cpu_count;
extern cpu_t *g_cpu_list;
