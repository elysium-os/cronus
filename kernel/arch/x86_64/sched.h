#pragma once

#include "arch/x86_64/cpu/cpu.h"

/// Initialize scheduler for CPU.
void x86_64_sched_init_cpu(x86_64_cpu_t *cpu);

/// Handoff a CPU for scheduling.
/// @warning The release enables interrupts for current and parked CPU's.
/// @param release If false, cpu will block until stage is set to SCHED. If true, will set stage to SCHED.
[[gnu::no_instrument_function]] [[noreturn]] void x86_64_sched_handoff_cpu(x86_64_cpu_t *cpu, bool release);
