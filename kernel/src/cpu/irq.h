#ifndef CPU_ISR_H
#define CPU_ISR_H

#include <stdint.h>

typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t int_no;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t userrsp;
    uint64_t ss;
} irq_frame_t;

typedef void (*interrupt_handler_t)(irq_frame_t *);

extern void (* g_irq_eoi)(uint8_t);

void irq_initialize();
void irq_register_handler(uint8_t id, interrupt_handler_t interrupt_handler);
void irq_handler(irq_frame_t *regs);
void irq_eoi(uint8_t interrupt);

extern void irq_32();
extern void irq_33();
extern void irq_34();
extern void irq_35();
extern void irq_36();
extern void irq_37();
extern void irq_38();
extern void irq_39();
extern void irq_40();
extern void irq_41();
extern void irq_42();
extern void irq_43();
extern void irq_44();
extern void irq_45();
extern void irq_46();
extern void irq_47();

#endif