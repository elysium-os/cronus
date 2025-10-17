#pragma once

#include <stdint.h>
#include "../../../include/arch/interrupt.h"

typedef struct [[gnu::packed]] arch_interrupt_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t int_no;
    uint64_t err_code, rip, cs, rflags, rsp, ss;
} arch_interrupt_frame_t;

/// Get the current interrupt state.
/// @returns true for enabled, false for disabled
static inline bool arch_interrupt_state() {
    uint64_t rflags;
    asm volatile("pushfq\npopq %0" : "=rm"(rflags));
    return (rflags & (1 << 9)) != 0;
}

/// Enable interrupts.
static inline void arch_interrupt_enable() {
    asm volatile("sti");
}

/// Disable interrupts.
static inline void arch_interrupt_disable() {
    asm volatile("cli");
}