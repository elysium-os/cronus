global x86_64_sched_context_switch
x86_64_sched_context_switch:
    push rbx
    push rbp
    push r15
    push r14
    push r13
    push r12

    mov qword [rdi + THREAD_RSP_OFFSET], rsp
    mov rsp, qword [rsi + THREAD_RSP_OFFSET]

    xor r12, r12
    mov ds, r12
    mov es, r12

    pop r12
    pop r13
    pop r14
    pop r15
    pop rbp
    pop rbx

    mov rax, rdi
    ret

global x86_64_sched_userspace_init
x86_64_sched_userspace_init:
    pop rcx ; return address

    cli
    swapgs

    pop rax ; stack pointer
    mov rsp, rax

    xor rbp, rbp
    xor rax, rax
    xor rbx, rbx
	xor rdx, rdx
	xor rsi, rsi
	xor rdi, rdi
	xor r8, r8
	xor r9, r9
	xor r10, r10
	xor r12, r12
	xor r13, r13
	xor r14, r14
	xor r15, r15

    mov r11, (1 << 9) | (1 << 1)
    o64 sysret
