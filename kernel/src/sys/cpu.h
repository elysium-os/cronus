#pragma once

#include "sched/thread.h"
#include "sys/ipl.h"

typedef struct cpu {
    struct thread *idle_thread;
} cpu_t;
