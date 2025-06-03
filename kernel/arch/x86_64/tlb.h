#pragma once

#include "stddef.h"
#include "stdint.h"

// Initialize TLB shootdown IPIs.
void x86_64_tlb_init_ipis();

/// Perform a TLB shootdown for a range of virtual memory.
void x86_64_tlb_shootdown(uintptr_t addr, size_t length);
