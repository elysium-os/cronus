; MMIO Reads
global mmio_read8
mmio_read8:
    xor rax, rax
    mov al, byte [rdi]
    ret

global mmio_read16
mmio_read16:
    xor rax, rax
    mov ax, word [rdi]
    ret

global mmio_read32
mmio_read32:
    xor rax, rax
    mov eax, dword [rdi]
    ret

global mmio_read64
mmio_read64:
    xor rax, rax
    mov rax, qword [rdi]
    ret

; MMIO Writes
global mmio_write8
mmio_write8:
    mov rax, rsi
    mov byte [rdi], al
    ret

global mmio_write16
mmio_write16:
    mov rax, rsi
    mov word [rdi], ax
    ret

global mmio_write32
mmio_write32:
    mov rax, rsi
    mov dword [rdi], eax
    ret

global mmio_write64
mmio_write64:
    mov rax, rsi
    mov qword [rdi], rax
    ret
