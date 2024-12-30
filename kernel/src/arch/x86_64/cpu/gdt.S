.global gdt_load

gdt_load:
    lgdt (%rdi)
    push %rsi
    lea .load_code(%rip), %rax
    push %rax
    lretq
.load_code:
    xor %rax, %rax

    mov %rdx, %ss
    mov %rax, %ds
    mov %rax, %es

    ret