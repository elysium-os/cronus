#pragma once

#include "arch/x86_64/cpu/cpu.h"

/**
 * @brief Initialize the scheduler.
 */
void x86_64_sched_init();

/**
 * @brief Initialize a CPU for scheduling.
 * @param release If false, cpu will block until stage is set to SCHED. If true, will set stage to SCHED.
 */
[[noreturn]] void x86_64_sched_init_cpu(x86_64_cpu_t *cpu, bool release);

/**
 * @brief Perform a context switch to the next thread.
 * @warning This essentially yields to the next thread, without the yield logic.
 */
void x86_64_sched_next();
