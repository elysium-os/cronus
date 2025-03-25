#pragma once

#include "sys/cpu.h"

/**
 * @brief Get the current CPU local.
 */
cpu_t *arch_cpu_current();

/**
 * @brief Get the active CPU count. Guaranteed to be at least the maximum id + 1.
 */
size_t arch_cpu_count();

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
