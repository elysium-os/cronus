#pragma once

#include <stdint.h>

/**
 * @brief Port in (byte).
 */
static inline uint8_t x86_64_port_inb(uint16_t port) {
    uint8_t result;
    asm volatile("inb %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

/**
 * @brief Port out (byte).
 */
static inline void x86_64_port_outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a" (value), "Nd" (port));
}

/**
 * @brief Port in (word).
 */
static inline uint32_t x86_64_port_inl(uint16_t port) {
    uint32_t result;
    asm volatile("inl %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

/**
 * @brief Port out (word).
 */
static inline void x86_64_port_outl(uint16_t port, uint32_t value) {
    asm volatile("outl %0, %1" : : "a" (value), "Nd" (port));
}