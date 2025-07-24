#pragma once

#include "sys/cpu.h"

/// Get the current CPU local.
cpu_t *arch_cpu_current();

/// Get cpu id. Guaranteed to be sequential.
size_t arch_cpu_id();

/// "Relax" the CPU.
void arch_cpu_relax();

/// Halt the CPU.
[[noreturn]] void arch_cpu_halt();
