#pragma once

#include "sched/thread.h"

/**
 * @brief Pick the next thread and perform a context switch to it.
 */
void arch_sched_yield(thread_state_t yield_state);

/**
 * @brief Create a new userspace thread.
 * @param ip userspace entry point
 * @param sp userspace stack pointer
 */
thread_t *arch_sched_thread_create_user(process_t *proc, uintptr_t ip, uintptr_t sp);

/**
 * @brief Create a new kernel thread.
 */
thread_t *arch_sched_thread_create_kernel(void (*func)());

/**
 * @brief Return the active thread on the current CPU.
 */
thread_t *arch_sched_thread_current();
