#pragma once

#include <stdint.h>

typedef enum {
    X86_64_MSR_APIC_BASE = 0x1B,
    X86_64_MSR_PAT = 0x277,
    X86_64_MSR_EFER = 0xC000'0080,
    X86_64_MSR_STAR = 0xC000'0081,
    X86_64_MSR_LSTAR = 0xC000'0082,
    X86_64_MSR_CSTAR = 0xC000'0083,
    X86_64_MSR_SFMASK = 0xC000'0084,
    X86_64_MSR_FS_BASE = 0xC000'0100,
    X86_64_MSR_GS_BASE = 0xC000'0101,
    X86_64_MSR_KERNEL_GS_BASE = 0xC000'0102
} x86_64_msr_t;

/**
 * @brief Read from machine specific register.
 */
static inline uint64_t x86_64_msr_read(uint64_t msr) {
    uint32_t low;
    uint32_t high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return low + ((uint64_t) high << 32);
}

/**
 * @brief Write to machine specific register.
 */
static inline void x86_64_msr_write(uint64_t msr, uint64_t value) {
    asm volatile("wrmsr" : : "a"((uint32_t) value), "d"((uint32_t) (value >> 32)), "c"(msr));
}
