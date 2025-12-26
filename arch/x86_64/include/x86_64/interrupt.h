#pragma once

#include "sys/interrupt.h"

#include <stdint.h>

#define X86_64_INTERRUPT_IS_FROM_USER(FRAME) (((FRAME)->cs & 3) != 0)

typedef void (*x86_64_interrupt_irq_eoi_t)(uint8_t);

extern x86_64_interrupt_irq_eoi_t g_x86_64_interrupt_irq_eoi;

/// Initialize IDT and interrupt management.
void x86_64_interrupt_init();

/// Set an IST for a given interrupt vector.
void x86_64_interrupt_set_ist(uint8_t vector, uint8_t ist);

/// Set a handler onto an interrupt vector.
/// @warning Will carelessly override existing handlers.
void x86_64_interrupt_set(uint8_t vector, void (*handler)(arch_interrupt_frame_t *frame));

/// Request a free interrupt vector.
/// @returns chosen interrupt vector, -1 on error
int x86_64_interrupt_request(interrupt_priority_t priority, void (*handler)(arch_interrupt_frame_t *frame));
