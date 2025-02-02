#pragma once

#include <sys/cpu.h>

/**
 * @brief Get the current CPU local.
 */
cpu_t *arch_cpu_current();

/**
 * @brief Get cpu id. Guaranteed to be sequential.
 */
size_t arch_cpu_id();

/**
 * @brief "Relax" the CPU.
 */
void arch_cpu_relax();

/**
 * @brief Halt the CPU.
 */
[[noreturn]] void arch_cpu_halt();
