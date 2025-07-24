#pragma once

#include "stddef.h"
#include "stdint.h"

/// Perform a TLB shootdown for a range of virtual memory.
void x86_64_tlb_shootdown(uintptr_t addr, size_t length);
