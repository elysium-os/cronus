.global memset
.global memcpy
.global memmove

.type memset, "function"
memset:
    mov %rdi, %r8
    mov %rdx, %rcx
    mov %rsi, %rax
    rep stosb
    mov %r8, %rax
    ret
.size memset, . - memset

.type memcpy, "function"
memcpy:
    mov %rdx, %rcx
    mov %rdi, %rax
    rep movsb
    ret
.size memcpy, . - memcpy

.type memmove, "function"
memmove:
    mov %rdx, %rcx
    mov %rdi, %rax
    cmp %rsi, %rdi
    jb .move
    lea -1(%rdi, %rcx), %rdi
    lea -1(%rsi, %rcx), %rsi
    std
.move:
    rep movsb
    cld
    ret
.size memmove, . - memmove