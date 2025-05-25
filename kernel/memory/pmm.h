#pragma once

#include "common/lock/spinlock.h"
#include "lib/list.h"

#include <stddef.h>
#include <stdint.h>

#define PMM_MAX_ORDER 7

#define PMM_ORDER_TO_PAGECOUNT(ORDER) (1llu << (ORDER))

#define PMM_FLAG_NONE (0)
#define PMM_FLAG_ZERO (1 << 0)
#define PMM_FLAG_ZONE_LOW (1 << 1)

typedef uint8_t pmm_flags_t;
typedef uint8_t pmm_order_t;

typedef struct {
    const char *name;
    uintptr_t start, end;

    spinlock_t lock;
    list_t lists[PMM_MAX_ORDER + 1];

    size_t total_page_count;
    size_t free_page_count;
} pmm_zone_t;

typedef struct pmm_block {
    list_node_t list_node; /* unallocated = used by pmm */
    uint8_t order     : 3;
    uint8_t max_order : 3;
    uint8_t free      : 1;
} pmm_block_t;

extern pmm_zone_t g_pmm_zone_normal;
extern pmm_zone_t g_pmm_zone_low;

/// Adds a block of memory to be managed by the PMM.
/// @param base Region base address
/// @param size Region size in bytes
/// @param used Count of pages already used
void pmm_region_add(uintptr_t base, size_t size, size_t used);

/// Allocates a block of size order^2 pages.
pmm_block_t *pmm_alloc(pmm_order_t order, pmm_flags_t flags);

/// Allocates the smallest block of size N^2 pages to fit size.
pmm_block_t *pmm_alloc_pages(size_t page_count, pmm_flags_t flags);

/// Allocates a block of memory the size of a page.
pmm_block_t *pmm_alloc_page(pmm_flags_t flags);

/// Frees a previously allocated block.
void pmm_free(pmm_block_t *block);
