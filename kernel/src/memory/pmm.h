#pragma once

#include "common/spinlock.h"
#include "lib/list.h"

#include <stddef.h>
#include <stdint.h>

#define PMM_MAX_ORDER 7

#define PMM_ZONE_COUNT (sizeof(typeof(g_pmm_zone_mask)) * 8)
#define PMM_ZONE_PRESENT(ZONE_INDEX) ((g_pmm_zone_mask & (1 << ZONE_INDEX)) != 0)

#define PMM_ZONE_NORMAL 0
#define PMM_ZONE_LOW 1

#define PMM_FLAG_NONE (0)
#define PMM_FLAG_ZERO (1 << 0)

typedef uint16_t pmm_flags_t;
typedef uint8_t pmm_zone_index_t;
typedef uint8_t pmm_order_t;

typedef struct {
    spinlock_t lock;
    list_t regions;
    list_t lists[PMM_MAX_ORDER + 1];
    size_t page_count;
    size_t free_count;
    uintptr_t start;
    uintptr_t end;
    const char *name;
} pmm_zone_t;

typedef struct pmm_page {
    /* unallocated = used by pmm, allocated = reserved for vmm */
    list_element_t list_elem;
    struct pmm_region *region;
    uintptr_t paddr;
    uint8_t order : 3;
    uint8_t free  : 1;
} pmm_page_t;

typedef struct pmm_region {
    list_element_t list_elem;
    pmm_zone_t *zone;
    uintptr_t base;
    size_t page_count;
    size_t free_count;
    pmm_page_t pages[];
} pmm_region_t;

extern uint8_t g_pmm_zone_mask;
extern pmm_zone_t g_pmm_zones[];

/**
 * @brief Register a memory zone.
 */
void pmm_zone_register(pmm_zone_index_t zone_index, const char *name, uintptr_t start, uintptr_t end);

/**
 * @brief Adds a block of memory to be managed by the PMM.
 * @param base region base address
 * @param size region size in bytes
 */
void pmm_region_add(uintptr_t base, size_t size);

/**
 * @brief Allocates a block of size order^2 pages.
 */
pmm_page_t *pmm_alloc(pmm_zone_index_t zone_index, pmm_order_t order, pmm_flags_t flags);

/**
 * @brief Allocates the smallest block of size N^2 pages to fit size.
 */
pmm_page_t *pmm_alloc_pages(pmm_zone_index_t zone_index, size_t page_count, pmm_flags_t flags);

/**
 * @brief Allocates a page of memory.
 */
pmm_page_t *pmm_alloc_page(pmm_zone_index_t zone_index, pmm_flags_t flags);

/**
 * @brief Frees a previously allocated page.
 */
void pmm_free(pmm_page_t *page);

/**
 * @brief Frees a previously allocated page by address.
 * @warning relatively expensive, has to find the region with said address
 */
void pmm_free_address(uintptr_t physical_address);
