; MMIO Reads
global arch_mmio_read8
arch_mmio_read8:
    xor rax, rax
    mov al, byte [rdi]
    ret

global arch_mmio_read16
arch_mmio_read16:
    xor rax, rax
    mov ax, word [rdi]
    ret

global arch_mmio_read32
arch_mmio_read32:
    xor rax, rax
    mov eax, dword [rdi]
    ret

global arch_mmio_read64
arch_mmio_read64:
    xor rax, rax
    mov rax, qword [rdi]
    ret

; MMIO Writes
global arch_mmio_write8
arch_mmio_write8:
    mov rax, rsi
    mov byte [rdi], al
    ret

global arch_mmio_write16
arch_mmio_write16:
    mov rax, rsi
    mov word [rdi], ax
    ret

global arch_mmio_write32
arch_mmio_write32:
    mov rax, rsi
    mov dword [rdi], eax
    ret

global arch_mmio_write64
arch_mmio_write64:
    mov rax, rsi
    mov qword [rdi], rax
    ret
