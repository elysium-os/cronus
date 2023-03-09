#ifndef CPU_EXCEPTIONS_H
#define CPU_EXCEPTIONS_H

#include <stdint.h>

typedef struct {
    uint64_t rdi, rsi, rbp, rsp, rdx, rcx, rbx, rax;
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t int_no, err_code;
    uint64_t rip, cs, eflags, useresp, ss;
} exception_frame_t;

void exceptions_initialize();
void exceptions_handler(exception_frame_t regs);

extern void exception_0();
extern void exception_1();
extern void exception_2();
extern void exception_3();
extern void exception_4();
extern void exception_5();
extern void exception_6();
extern void exception_7();
extern void exception_8();
extern void exception_9();
extern void exception_10();
extern void exception_11();
extern void exception_12();
extern void exception_13();
extern void exception_14();
extern void exception_15();
extern void exception_16();
extern void exception_17();
extern void exception_18();
extern void exception_19();
extern void exception_20();
extern void exception_21();
extern void exception_22();
extern void exception_23();
extern void exception_24();
extern void exception_25();
extern void exception_26();
extern void exception_27();
extern void exception_28();
extern void exception_29();
extern void exception_30();
extern void exception_31();

#endif