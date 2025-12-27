#pragma once

#include <stddef.h>
#include <stdint.h>

extern bool g_earlymem_active;

/// Allocate a page from early memory.
uintptr_t earlymem_alloc_page();

/// Free a page to early memory.
void earlymem_free_page(uintptr_t address);

/// Allocate a block of memory <= page size (returns a physical address).
uintptr_t earlymem_alloc(size_t size);
