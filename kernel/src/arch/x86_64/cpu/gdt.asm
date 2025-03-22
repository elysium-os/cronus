global x86_64_gdt_load
x86_64_gdt_load:
    lgdt [rdi]
    push rsi
    lea rax, [rel .load_code]
    push rax
    retf
.load_code:
    mov ss, rdx
    xor rax, rax
    mov ds, rax
    mov es, rax
    ret
