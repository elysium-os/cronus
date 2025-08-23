#pragma once

#include "sched/thread.h"

/// Handoff current CPU to scheduler.
[[noreturn]] void sched_handoff_cpu();

/// Create a new userspace thread.
/// @param ip Userspace entry point
/// @param sp Userspace stack pointer
thread_t *sched_thread_create_user(process_t *proc, uintptr_t ip, uintptr_t sp);

/// Create a new kernel thread.
thread_t *sched_thread_create_kernel(void (*func)());

/// Return the active thread on the current CPU.
thread_t *sched_thread_current();

/// Issue preemption timer.
void sched_preempt();

/// Perform a context switch.
void sched_context_switch(thread_t *current, thread_t *next);
