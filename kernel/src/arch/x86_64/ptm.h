#pragma once

#include "memory/vm.h"

#include "arch/x86_64/interrupt.h"

/**
 * @brief Initialize page table manager.
 */
vm_address_space_t *x86_64_ptm_init();

/**
 * @brief Handles page faults and passes to the architecture agnostic handler.
 */
void x86_64_ptm_page_fault_handler(x86_64_interrupt_frame_t *frame);
