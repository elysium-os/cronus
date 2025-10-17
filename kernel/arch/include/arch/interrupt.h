#pragma once

struct arch_interrupt_frame;
enum interrupt_priority;

/// Request a free interrupt vector and register a handler.
/// @returns chosen interrupt vector, -1 on error
int interrupt_request(enum interrupt_priority priority, void (*handler)(struct arch_interrupt_frame *frame));
