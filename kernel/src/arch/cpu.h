#pragma once

#include <sys/cpu.h>

/**
 * @brief Get the current CPU local.
 */
cpu_t *arch_cpu_current();

/**
 * @brief "Relax" the CPU.
 */
void arch_cpu_relax();

/**
 * @brief Halt the CPU.
 */
[[noreturn]] void arch_cpu_halt();
