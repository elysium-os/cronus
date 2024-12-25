global memset
global memcpy
global memmove

memset:
    push rdi
    mov rax, rsi
    mov rcx, rdx
    rep stosb
    pop rax
    ret

memcpy:
    mov rcx, rdx
    mov rax, rdi
    rep movsb
    ret

memmove:
    mov rcx, rdx
    mov rax, rdi
    cmp rdi, rsi
    jb .move
    add rdi, rcx
    add rsi, rcx
    dec rdi
    dec rsi
    std
.move:
    rep movsb
    cld
    ret