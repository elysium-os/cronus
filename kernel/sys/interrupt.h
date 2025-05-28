#pragma once

#include "arch/interrupt.h"

typedef bool interrupt_state_t;

typedef enum interrupt_priority {
    INTERRUPT_PRIORITY_LOW,
    INTERRUPT_PRIORITY_NORMAL,
    INTERRUPT_PRIORITY_EVENT,
    INTERRUPT_PRIORITY_CRITICAL
} interrupt_priority_t;

/// Mask interrupts and return the previous state.
static inline interrupt_state_t interrupt_state_mask() {
    interrupt_state_t previous_state = arch_interrupt_state();
    arch_interrupt_disable();
    return previous_state;
}

/// Restore interrupt state.
static inline void interrupt_state_restore(interrupt_state_t state) {
    interrupt_state_t current_state = arch_interrupt_state();
    if(current_state == state) return;

    if(state) {
        arch_interrupt_enable();
    } else {
        arch_interrupt_disable();
    }
}
