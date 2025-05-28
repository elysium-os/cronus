#pragma once

#include "sched/sched.h"

typedef struct cpu {
    sched_t sched;
    rb_tree_t events;
} cpu_t;
