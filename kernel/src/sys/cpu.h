#pragma once

#include "sched/thread.h"

typedef struct cpu {
    struct thread *idle_thread;
} cpu_t;
