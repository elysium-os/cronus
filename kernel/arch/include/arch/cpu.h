#pragma once

#include "sys/cpu.h"

/// Get the current CPU local.
cpu_t *cpu_current();

/// Get cpu id. Guaranteed to be sequential.
size_t cpu_id();

/// "Relax" the CPU.
void cpu_relax();

/// Halt the CPU.
[[noreturn]] void cpu_halt();
