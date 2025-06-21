#pragma once

#include "lib/list.h"
#include "sched/sched.h"

#include <stdint.h>

typedef struct cpu {
    sched_t sched;
    rb_tree_t events;
    list_t dw_items;
    struct {
        uint64_t in_interrupt_hard : 1;
        uint64_t in_interrupt_soft : 1;
    } flags;
} cpu_t;
