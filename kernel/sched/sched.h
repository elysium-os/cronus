#pragma once

#include "common/lock/spinlock.h"
#include "lib/list.h"
#include "sched/thread.h"

typedef struct sched {
    spinlock_t lock;
    list_t thread_queue;

    struct thread *idle_thread;
} sched_t;

/**
 * @brief Schedule a thread.
 */
void sched_thread_schedule(struct thread *thread);

/**
 * @brief Retrieve the next thread off of scheduler.
 */
struct thread *sched_thread_next(sched_t *sched);

/**
 * @brief Yield.
 */
void sched_yield(enum thread_state yield_state);

/**
 * @brief Called when a thread is dropped by a CPU.
 */
void internal_sched_thread_drop(struct thread *thread);
