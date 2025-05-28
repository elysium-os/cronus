#pragma once

#include "sys/interrupt.h"

enum interrupt_priority;

typedef void (*arch_interrupt_handler_t)();

/// Get the current interrupt state.
/// @returns true for enabled, false for disabled
bool arch_interrupt_state();

/// Enable interrupts.
void arch_interrupt_enable();

/// Disable interrupts.
void arch_interrupt_disable();

/// Request a free interrupt vector and register a handler.
/// @returns chosen interrupt vector, -1 on error
int arch_interrupt_request(enum interrupt_priority priority, arch_interrupt_handler_t handler);
