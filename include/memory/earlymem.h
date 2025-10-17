#pragma once

#include "lib/list.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uintptr_t base;
    size_t length;
    size_t hint;
    list_node_t list_node;
} earlymem_region_t;

extern bool g_earlymem_active;
extern list_t g_earlymem_regions;

/// Add a new memory region.
void earlymem_region_add(uintptr_t address, size_t length);

/// Check whether a page in a region is free.
bool earlymem_region_isfree(earlymem_region_t *region, size_t offset);

/// Allocate a page from early memory.
uintptr_t earlymem_alloc_page();

/// Free a page to early memory.
void earlymem_free_page(uintptr_t address);

/// Allocate a block of memory <= page size (returns a physical address).
uintptr_t earlymem_alloc(size_t size);
