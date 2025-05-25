#pragma once

#include <stdint.h>

/// Port in (byte).
static inline uint8_t x86_64_port_inb(uint16_t port) {
    uint8_t result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/// Port out (byte).
static inline void x86_64_port_outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

/// Port in (word).
static inline uint16_t x86_64_port_inw(uint16_t port) {
    uint16_t result;
    asm volatile("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/// Port out (word).
static inline void x86_64_port_outw(uint16_t port, uint16_t value) {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

/// Port in (dword).
static inline uint32_t x86_64_port_ind(uint16_t port) {
    uint32_t result;
    asm volatile("inl %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/// Port out (dword).
static inline void x86_64_port_outd(uint16_t port, uint32_t value) {
    asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}
