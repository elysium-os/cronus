#pragma once

#include "sys/time.h"

#include <stdint.h>

#define X86_64_LAPIC_IPI_ASSERT (1 << 14)

typedef enum : uint32_t {
    X86_64_LAPIC_TIMER_DIVISOR_1 = 0b1011,
    X86_64_LAPIC_TIMER_DIVISOR_2 = 0b0000,
    X86_64_LAPIC_TIMER_DIVISOR_4 = 0b0001,
    X86_64_LAPIC_TIMER_DIVISOR_8 = 0b0010,
    X86_64_LAPIC_TIMER_DIVISOR_16 = 0b0011,
    X86_64_LAPIC_TIMER_DIVISOR_32 = 0b1000,
    X86_64_LAPIC_TIMER_DIVISOR_64 = 0b1001,
    X86_64_LAPIC_TIMER_DIVISOR_128 = 0b1010,
} x86_64_lapic_timer_divisor_t;

typedef enum : uint32_t {
    X86_64_LAPIC_TIMER_TYPE_ONESHOT = 0b00 << 17,
    X86_64_LAPIC_TIMER_TYPE_PERIODIC = 0b01 << 17,
    X86_64_LAPIC_TIMER_TYPE_DEADLINE = 0b11 << 17,
} x86_64_lapic_timer_type_t;

/// Initialize the local apic.
void x86_64_lapic_init();

/// Get the local apic id of the current core.
uint32_t x86_64_lapic_id();

/// Issue an end of interrupt.
void x86_64_lapic_eoi(uint8_t interrupt_vector);

/// Issue an IPI.
/// @param vec Interrupt vector & flags
void x86_64_lapic_ipi(uint32_t lapic_id, uint32_t vec);

/// Setup the timer. This does not start it, that is done separately.
void x86_64_lapic_timer_setup(x86_64_lapic_timer_type_t type, bool mask_interrupt, uint8_t interrupt_vector, x86_64_lapic_timer_divisor_t divisor);

/// Starts the timer.
void x86_64_lapic_timer_start(uint64_t ticks);

/// Stops the timer.
void x86_64_lapic_timer_stop();

/// Reads the current counter in the timer.
uint32_t x86_64_lapic_timer_read();
