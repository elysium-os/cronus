#pragma once

/**
 * @brief Get the current interrupt state.
 * @returns true for enabled, false for disabled
 */
bool arch_interrupt_state();

/**
 * @brief Enable interrupts.
 */
void arch_interrupt_enable();

/**
 * @brief Disable interrupts.
 */
void arch_interrupt_disable();
