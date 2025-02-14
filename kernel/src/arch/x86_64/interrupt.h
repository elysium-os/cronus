#pragma once

#include "sys/ipl.h"

#include <stdint.h>

typedef struct [[gnu::packed]] {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t int_no;
    uint64_t err_code, rip, cs, rflags, rsp, ss;
} x86_64_interrupt_frame_t;

typedef void (*x86_64_interrupt_handler_t)(x86_64_interrupt_frame_t *frame);
typedef void (*x86_64_interrupt_irq_eoi_t)(uint8_t);

extern x86_64_interrupt_irq_eoi_t g_x86_64_interrupt_irq_eoi;

/**
 * @brief Initialize IDT and interrupt management.
 */
void x86_64_interrupt_init();

/**
 * @brief Load the IDT.
 */
void x86_64_interrupt_load_idt();

/**
 * @brief Set a handler onto an interrupt vector.
 * @warning Will carelessly override existing handlers.
 */
void x86_64_interrupt_set(uint8_t vector, x86_64_interrupt_handler_t handler);

/**
 * @brief Request a free interrupt vector.
 * @return chosen interrupt vector, -1 on error
 */
int x86_64_interrupt_request(ipl_t ipl, x86_64_interrupt_handler_t handler);
