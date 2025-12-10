global mem_set
mem_set:
    mov rcx, rdx
    mov rax, rsi
    rep stosb
    ret

global mem_copy
mem_copy:
    mov rcx, rdx
    rep movsb
    ret

global mem_move
mem_move:
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

global mem_clear
mem_clear:
    xor rax, rax
    mov rcx, rsi
    rep stosb
    ret
