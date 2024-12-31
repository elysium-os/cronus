.global memset
.global memcpy
.global memmove
.global memclear

.type memset, "function"
memset:
    mov %rdx, %rcx
    mov %rsi, %rax
    rep stosb
    ret
.size memset, . - memset

.type memcpy, "function"
memcpy:
    mov %rdx, %rcx
    rep movsb
    ret
.size memcpy, . - memcpy

.type memmove, "function"
memmove:
    mov %rdx, %rcx
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

.type memclear, "function"
memclear:
    xor %rax, %rax
    mov %rsi, %rcx
    rep stosb
    ret
.size memclear, . - memclear