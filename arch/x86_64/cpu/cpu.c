#include "arch/cpu.h"

#include "sys/init.h"
#include "x86_64/cpu/cr.h"
#include "x86_64/cpu/msr.h"

#include <stdint.h>

void arch_cpu_local_load(void *ptr) {
    x86_64_msr_write(X86_64_MSR_GS_BASE, (uintptr_t) ptr);
}

/// Initialize the Page Attribute Table (PAT) for the current CPU.
/// The PAT is configured in cronus as following:
///  - PA0: WB  (Write Back)
///  - PA1: WT  (Write Through)
///  - PA2: UC- (Uncached)
///  - PA3: UC  (Uncacheable)
///  - PA4: WB  (Write Back)
///  - PA5: WP  (Write Protected)
///  - PA6: WC  (Write Combining)
///  - PA7: UC  (Uncacheable)
INIT_TARGET(cpu_pat, INIT_STAGE_EARLY, INIT_SCOPE_ALL, INIT_DEPS()) {
    uint64_t pat = x86_64_msr_read(X86_64_MSR_PAT);
    pat &= ~(((uint64_t) 0b111 << 48) | ((uint64_t) 0b111 << 40));
    pat |= ((uint64_t) 0x1 << 48) | ((uint64_t) 0x5 << 40);
    x86_64_msr_write(X86_64_MSR_PAT, pat);
}

INIT_TARGET(cpu, INIT_STAGE_EARLY, INIT_SCOPE_ALL, INIT_DEPS()) {
    uint64_t cr4 = x86_64_cr4_read();
    cr4 |= 1 << 7; /* CR4.PGE */
    x86_64_cr4_write(cr4);
}
