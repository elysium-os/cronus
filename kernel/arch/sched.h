#pragma once

#include "sched/thread.h"

/// Create a new userspace thread.
/// @param ip Userspace entry point
/// @param sp Userspace stack pointer
thread_t *arch_sched_thread_create_user(process_t *proc, uintptr_t ip, uintptr_t sp);

/// Create a new kernel thread.
thread_t *arch_sched_thread_create_kernel(void (*func)());

/// Return the active thread on the current CPU.
thread_t *arch_sched_thread_current();

/// Issue preemption timer.
void arch_sched_preempt();

/// Perform a context switch.
void arch_sched_context_switch(thread_t *current, thread_t *next);
