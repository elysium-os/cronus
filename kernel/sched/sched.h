#pragma once

#include "sched/thread.h"

/**
 * @brief Schedule a thread.
 */
void sched_thread_schedule(thread_t *thread);

/**
 * @brief Retrieve the next thread for execution.
 */
thread_t *sched_thread_next();

/**
 * @brief Called when a thread is dropped by a CPU.
 */
void sched_thread_drop(thread_t *thread);
