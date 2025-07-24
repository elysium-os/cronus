#pragma once

#include <stdint.h>

#define DEFINE_WRITE(REGISTER)                                           \
    static inline void x86_64_##REGISTER##_write(uint64_t value) {       \
        asm volatile("movq %0, %%" #REGISTER : : "r"(value) : "memory"); \
    }

#define DEFINE_READ(REGISTER)                                   \
    static inline uint64_t x86_64_##REGISTER##_read() {         \
        uint64_t value;                                         \
        asm volatile("movq %%" #REGISTER ", %0" : "=r"(value)); \
        return value;                                           \
    }

DEFINE_READ(cr0)
DEFINE_WRITE(cr0)

DEFINE_READ(cr2)

DEFINE_READ(cr3)
DEFINE_WRITE(cr3)

DEFINE_READ(cr4)
DEFINE_WRITE(cr4)

DEFINE_READ(cr8)
DEFINE_WRITE(cr8)

#undef DEFINE_READ
#undef DEFINE_WRITE
