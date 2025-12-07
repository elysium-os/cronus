#include "arch/cpu.h"

#include "x86_64/cpu/msr.h"

void arch_cpu_local_write(void *ptr) {
    x86_64_msr_write(X86_64_MSR_GS_BASE, (uintptr_t) ptr);
}
