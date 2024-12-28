#pragma once

#include "memory/vm.h"

#include <stdint.h>

/**
 * @brief Load a virtual address space.
 */
void arch_ptm_load_address_space(vm_address_space_t *address_space);

/**
 * @brief Map a virtual address to a physical address.
 */
void arch_ptm_map(vm_address_space_t *address_space, uintptr_t vaddr, uintptr_t paddr, vm_protection_t prot, vm_cache_t cache, vm_privilege_t privilege, bool global);

/**
 * @brief Unmap a virtual address from address space.
 */
void arch_ptm_unmap(vm_address_space_t *address_space, uintptr_t vaddr);

/**
 * @brief Translate a virtual address to a physical address.
 * @param out physical address
 * @returns true on success
 */
bool arch_ptm_physical(vm_address_space_t *address_space, uintptr_t vaddr, uintptr_t *out);