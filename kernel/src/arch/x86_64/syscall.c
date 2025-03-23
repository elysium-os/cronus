#include "arch/x86_64/cpu/gdt.h"
#include "arch/x86_64/cpu/msr.h"

#define MSR_EFER_SCE (1 << 0)

extern void x86_64_syscall_entry();

static_assert(X86_64_GDT_SELECTOR_DATA64_RING3 + 8 == X86_64_GDT_SELECTOR_CODE64_RING3); // ensure GDT conforms to sysret

void x86_64_syscall_init_cpu() {
    x86_64_msr_write(X86_64_MSR_EFER, x86_64_msr_read(X86_64_MSR_EFER) | MSR_EFER_SCE);
    x86_64_msr_write(X86_64_MSR_STAR, ((uint64_t) X86_64_GDT_SELECTOR_CODE64_RING0 << 32) | ((uint64_t) (X86_64_GDT_SELECTOR_DATA64_RING3 - 8) << 48));
    x86_64_msr_write(X86_64_MSR_LSTAR, (uint64_t) x86_64_syscall_entry);
    x86_64_msr_write(X86_64_MSR_SFMASK, x86_64_msr_read(X86_64_MSR_SFMASK) | (1 << 9));
}
