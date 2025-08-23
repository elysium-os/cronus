#pragma once

#include "lib/param.h"
#include "memory/vm.h"

#include <stdint.h>

/// Create a new address space.
/// @warning Depends on heap.
vm_address_space_t *ptm_address_space_create();

/// Load a virtual address space.
void ptm_load_address_space(vm_address_space_t *address_space);

/// Map virtual addresses to physical addresses.
void ptm_map(vm_address_space_t *address_space, uintptr_t vaddr, uintptr_t paddr, size_t length, vm_protection_t prot, vm_cache_t cache, vm_privilege_t privilege, bool global);

// Rewrite flags for given addresses.
void ptm_rewrite(vm_address_space_t *address_space, uintptr_t vaddr, size_t length, vm_protection_t prot, vm_cache_t cache, vm_privilege_t privilege, bool global);

/// Unmap virtual addresses from address space.
void ptm_unmap(vm_address_space_t *address_space, uintptr_t vaddr, size_t length);

/// Translate a virtual address to a physical address.
/// @returns true on success
bool ptm_physical(vm_address_space_t *address_space, uintptr_t vaddr, PARAM_OUT(uintptr_t *) paddr);
