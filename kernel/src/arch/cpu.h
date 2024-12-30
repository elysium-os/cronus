#pragma once

/**
 * @brief "Relax" the CPU.
 */
void arch_cpu_relax();

/**
 * @brief Halt the CPU.
 */
[[noreturn]] void arch_cpu_halt();
