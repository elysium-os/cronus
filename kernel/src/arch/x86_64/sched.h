#pragma once

#include "arch/x86_64/cpu/cpu.h"

/**
 * @brief Initialize the scheduler.
 */
void x86_64_sched_init();

/**
 * @brief Initialize a CPU for scheduling.
 * @warning The release enables interrupts for current and parked CPU's.
 * @param release If false, cpu will block until stage is set to SCHED. If true, will set stage to SCHED.
 */
[[noreturn]] void x86_64_sched_init_cpu(x86_64_cpu_t *cpu, bool release);
