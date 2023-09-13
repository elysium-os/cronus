#pragma once
#include <stdint.h>

#define PIT_FREQ 1193182

/**
 * @brief Initialized the PIT
 *
 * @param divisor PIT divisor
 */
void pit_initialize(uint16_t divisor);

/**
 * @brief Retrieve current PIT count
 *
 * @return Current PIT count
 */
uint16_t pit_count();