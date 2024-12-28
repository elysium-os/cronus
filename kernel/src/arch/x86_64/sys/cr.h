#pragma once

#include <stdint.h>

static inline uint64_t x86_64_cr2_read() {
    uint64_t value;
    asm volatile("movq %%cr2, %0" : "=r"(value));
    return value;
}

static inline uint64_t x86_64_cr3_read() {
    uint64_t value;
    asm volatile("movq %%cr3, %0" : "=r"(value));
    return value;
}

static inline void x86_64_cr3_write(uint64_t value) {
    asm volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

static inline uint64_t x86_64_cr4_read() {
    uint64_t value;
    asm volatile("movq %%cr4, %0" : "=r"(value));
    return value;
}

static inline void x86_64_cr4_write(uint64_t value) {
    asm volatile("mov %0, %%cr4" : : "r"(value) : "memory");
}

static inline uint64_t x86_64_cr8_read() {
    uint64_t value;
    asm volatile("movq %%cr8, %0" : "=r"(value));
    return value;
}

static inline void x86_64_cr8_write(uint64_t value) {
    asm volatile("mov %0, %%cr8" : : "r"(value));
}
