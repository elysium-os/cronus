global memset
memset:
    mov rcx, rdx
    mov rax, rsi
    rep stosb
    ret

global memcpy
memcpy:
    mov rcx, rdx
    rep movsb
    ret

global memmove
memmove:
    mov rcx, rdx
    cmp rdi, rsi
    jb .move
    lea rdi, [rdi + rcx - 1]
    lea rsi, [rsi + rcx - 1]
    std
.move:
    rep movsb
    cld
    ret

global memclear
memclear:
    xor rax, rax
    mov rcx, rsi
    rep stosb
    ret