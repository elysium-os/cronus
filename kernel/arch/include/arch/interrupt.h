#pragma once

#include "sys/interrupt.h"

enum interrupt_priority;

typedef void (*interrupt_handler_t)();

/// Get the current interrupt state.
/// @returns true for enabled, false for disabled
bool interrupt_state();

/// Enable interrupts.
void interrupt_enable();

/// Disable interrupts.
void interrupt_disable();

/// Request a free interrupt vector and register a handler.
/// @returns chosen interrupt vector, -1 on error
int interrupt_request(enum interrupt_priority priority, interrupt_handler_t handler);
