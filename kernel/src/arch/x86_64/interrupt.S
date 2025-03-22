.altmacro
.section .text

.macro SWAPGS_CONDITIONAL
    testq $3, 24(%rsp)
    jz 1f
    swapgs
1:
.endm

.type isr_stub, "function"
isr_stub:
    cld

    SWAPGS_CONDITIONAL

    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rbp
    push %rsi
    push %rdi
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    xor %rbp, %rbp
    mov %rsp, %rdi
    call x86_64_interrupt_handler

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rbp
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    SWAPGS_CONDITIONAL

    add $16, %rsp
    iretq

.macro ISR vec err
.type isr_\vec, "function"
isr_\vec:
.if \err == 0
    push $0
.endif
    push $\vec
    jmp isr_stub
.endm

ISR 0, 0
ISR 1, 0
ISR 2, 0
ISR 3, 0
ISR 4, 0
ISR 5, 0
ISR 6, 0
ISR 7, 0
ISR 8, 1
ISR 9, 0
ISR 10, 1
ISR 11, 1
ISR 12, 1
ISR 13, 1
ISR 14, 1
ISR 15, 0
ISR 16, 0
ISR 17, 1
ISR 18, 0
ISR 19, 0
ISR 20, 0
ISR 21, 1
ISR 22, 0
ISR 23, 0
ISR 24, 1
ISR 25, 0
ISR 26, 0
ISR 27, 0
ISR 28, 1
ISR 29, 1
ISR 30, 1
ISR 31, 0

.set i, 32
.rept 224
ISR %i, 0
.set i, i+1
.endr

// ISR stub table
.section .rodata

.macro ISR_ARR vec
.quad isr_\vec
.endm

.global g_x86_64_isr_stubs
.type g_x86_64_isr_stubs, "object"
g_x86_64_isr_stubs:
.set i, 0
.rept 256
ISR_ARR %i
.set i, i+1
.endr
.size g_x86_64_isr_stubs, . - g_x86_64_isr_stubs