#pragma once

#include "common/lock/spinlock.h"
#include "lib/list.h"
#include "sched/thread.h"

typedef struct sched {
    spinlock_t lock;
    list_t thread_queue;

    bool preemption_enabled;

    struct thread *idle_thread;
} sched_t;

/// Schedule a thread.
void sched_thread_schedule(struct thread *thread);

/// Retrieve the next thread off of scheduler.
struct thread *sched_thread_next(sched_t *sched);

/// Yield.
void sched_yield(enum thread_state yield_state);

/// Called when a thread is dropped by a CPU.
void internal_sched_thread_drop(struct thread *thread);
