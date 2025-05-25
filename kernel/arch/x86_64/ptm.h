#pragma once

#include "memory/vm.h"

#include "arch/x86_64/interrupt.h"

extern uintptr_t (*g_x86_64_ptm_phys_allocator)();

/// Initialize page table manager.
vm_address_space_t *x86_64_ptm_init();

/// Handles page faults and passes to the architecture agnostic handler.
void x86_64_ptm_page_fault_handler(x86_64_interrupt_frame_t *frame);
