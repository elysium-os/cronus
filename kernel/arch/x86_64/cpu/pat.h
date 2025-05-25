#pragma once

#include "arch/x86_64/cpu/msr.h"

#include <stdint.h>

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
static inline void pat_init() {
    uint64_t pat = x86_64_msr_read(X86_64_MSR_PAT);
    pat &= ~(((uint64_t) 0b111 << 48) | ((uint64_t) 0b111 << 40));
    pat |= ((uint64_t) 0x1 << 48) | ((uint64_t) 0x5 << 40);
    x86_64_msr_write(X86_64_MSR_PAT, pat);
}
