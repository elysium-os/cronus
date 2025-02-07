#pragma once

#include "memory/vm.h"
#include "sched/process.h"
#include "sched/thread.h"

/**
 * @brief Create a process.
 */
process_t *sched_process_create(vm_address_space_t *address_space);

/**
 * @brief Destroy a process.
 * @warning Assumes you have acquired the process lock and threads are not in queue.
 */
void sched_process_destroy(process_t *proc);

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
