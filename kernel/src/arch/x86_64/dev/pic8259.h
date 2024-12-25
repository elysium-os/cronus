#pragma once

#include <stdint.h>

/**
 * @brief Remap the 8529 PIC irqs.
 */
void x86_64_pic8259_remap();

/**
 * @brief Disable the 8259 PIC (mask all irqs).
 */
void x86_64_pic8259_disable();

/**
 * @brief Issue an end of interrupt to the 8259 PIC.
 */
void x86_64_pic8259_eoi(uint8_t interrupt_vector);