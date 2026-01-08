#pragma once

#include "common/lock/spinlock.h"
#include "lib/list.h"
#include "sched/thread.h"

typedef struct sched {
    spinlock_t lock;
    list_t thread_queue;
    struct thread *idle_thread;
} sched_t;

extern sched_t *g_sched;

/// Schedule a thread.
void sched_thread_schedule(struct thread *thread);

/// Retrieve the next thread off of scheduler.
struct thread *sched_thread_next(sched_t *sched);

/// Yield.
void sched_yield(enum thread_state yield_state);

/// Increment the preempt counter, effectively disables preemtion.
/// Meant to be paired up with a `dec` call.
void sched_preempt_inc();

/// Decrement the preempt counter.
/// If it reaches zero preemptions are enabled and the yield_immediately flag processed.
void sched_preempt_dec();

/// Called when a thread is dropped by a CPU.
void internal_sched_thread_drop(struct thread *thread);
