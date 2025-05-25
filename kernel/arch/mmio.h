#pragma once

#include <stdint.h>

/**
 * @brief Generic MMIO, 8 bit read.
 */
uint8_t mmio_read8(volatile void *src);

/**
 * @brief Generic MMIO, 16 bit read.
 */
uint16_t mmio_read16(volatile void *src);

/**
 * @brief Generic MMIO, 32 bit read.
 */
uint32_t mmio_read32(volatile void *src);

/**
 * @brief Generic MMIO, 64 bit read.
 */
uint64_t mmio_read64(volatile void *src);


/**
 * @brief Generic MMIO, 8 bit write.
 */
void mmio_write8(volatile void *dest, uint8_t value);

/**
 * @brief Generic MMIO, 16 bit write.
 */
void mmio_write16(volatile void *dest, uint16_t value);

/**
 * @brief Generic MMIO, 32 bit write.
 */
void mmio_write32(volatile void *dest, uint32_t value);

/**
 * @brief Generic MMIO, 64 bit write.
 */
void mmio_write64(volatile void *dest, uint64_t value);
