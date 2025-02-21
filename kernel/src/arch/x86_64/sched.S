#define THREAD_RSP_OFFSET 0

.global x86_64_sched_context_switch
.global x86_64_sched_userspace_init

.type x86_64_sched_context_switch, "function"
x86_64_sched_context_switch:
    push %rbx
    push %rbp
    push %r15
    push %r14
    push %r13
    push %r12

    mov %rsp, THREAD_RSP_OFFSET(%rdi)
    mov THREAD_RSP_OFFSET(%rsi), %rsp

    xor %r12, %r12
    mov %ds, %r12
    mov %es, %r12

    pop %r12
    pop %r13
    pop %r14
    pop %r15
    pop %rbp
    pop %rbx

    mov %rdi, %rax
    ret

.type x86_64_sched_userspace_init, "function"
x86_64_sched_userspace_init:
    pop %rcx

    cli
    swapgs

    pop %rax
    mov %rax, %rsp

    xor %rbp, %rbp
    xor %rax, %rax
    xor %rbx, %rbx
    xor %rdx, %rdx
    xor %rsi, %rsi
    xor %rdi, %rdi
    xor %r8, %r8
    xor %r9, %r9
    xor %r10, %r10
    xor %r12, %r12
    xor %r13, %r13
    xor %r14, %r14
    xor %r15, %r15

    mov $((1 << 9) | (1 << 1)), %r11
    sysretq