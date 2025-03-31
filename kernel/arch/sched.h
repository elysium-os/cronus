#pragma once

#include "sched/thread.h"

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

/**
 * @brief Issue preemption timer.
 */
void arch_sched_preempt();

/**
 * @brief Perform a context switch.
 */
void arch_sched_context_switch(thread_t *current, thread_t *next);
