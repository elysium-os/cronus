#pragma once

#include "common/lock/spinlock.h"
#include "lib/list.h"
#include "sched/thread.h"

typedef enum : bool {
    SCHED_PREEMPT_STATE_ENABLED = true,
    SCHED_PREEMPT_STATE_DISABLED = false
} sched_preempt_state_t;

typedef struct sched {
    spinlock_t lock;
    list_t thread_queue;

    struct {
        uint64_t preempt           : 1;
        uint64_t yield_immediately : 1;
    } status;

    struct thread *idle_thread;
} sched_t;

/// Schedule a thread.
void sched_thread_schedule(struct thread *thread);

/// Retrieve the next thread off of scheduler.
struct thread *sched_thread_next(sched_t *sched);

/// Yield.
void sched_yield(enum thread_state yield_state);

/// Disable preemption and return the previous state.
sched_preempt_state_t sched_preempt_disable();

/// Restore a preemption state. Meant to be paired up with disable.
void sched_preempt_restore(sched_preempt_state_t state);

/// Set preemption state and return the previous state.
sched_preempt_state_t sched_preempt_set(sched_preempt_state_t state);

/// Called when a thread is dropped by a CPU.
void internal_sched_thread_drop(struct thread *thread);
