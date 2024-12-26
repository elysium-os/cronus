#pragma once

#include <stdint.h>

#define X86_64_LAPIC_IPI_ASSERT (1 << 14)

/**
 * @brief Initialize and enable the local apic for the current core.
 */
void x86_64_lapic_initialize();

/**
 * @brief Get the local apic id of the current core.
 */
uint32_t x86_64_lapic_id();

/**
 * @brief Issue an end of interrupt.
 */
void x86_64_lapic_eoi(uint8_t interrupt_vector);

/**
 * @brief Issue an IPI.
 * @param vec interrupt vector & flags
 */
void x86_64_lapic_ipi(uint32_t lapic_id, uint32_t vec);

