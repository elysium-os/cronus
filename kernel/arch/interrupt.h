#pragma once

/// Get the current interrupt state.
/// @returns true for enabled, false for disabled
bool arch_interrupt_state();

/// Enable interrupts.
void arch_interrupt_enable();

/// Disable interrupts.
void arch_interrupt_disable();
