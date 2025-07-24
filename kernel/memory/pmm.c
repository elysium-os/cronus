#include "pmm.h"

#include "arch/mem.h"
#include "arch/page.h"
#include "common/assert.h"
#include "common/log.h"
#include "lib/mem.h"
#include "memory/hhdm.h"
#include "memory/page.h"

#include <stdint.h>

#define BLOCK_PADDR(BLOCK) PAGE_PADDR(PAGE_FROM_BLOCK(BLOCK))

pmm_zone_t g_pmm_zone_low = {
    .name = "LOW",
    .start = ARCH_PAGE_GRANULARITY,
    .end = ARCH_MEM_LOW_SIZE,
    .total_page_count = 0,
    .free_page_count = 0,
    .lock = SPINLOCK_INIT,
    .lists = { [0 ... PMM_MAX_ORDER] = LIST_INIT },
};

pmm_zone_t g_pmm_zone_normal = {
    .name = "NORMAL",
    .start = ARCH_MEM_LOW_SIZE,
    .end = ((UINTPTR_MAX) / ARCH_PAGE_GRANULARITY) * ARCH_PAGE_GRANULARITY,
    .total_page_count = 0,
    .free_page_count = 0,
    .lock = SPINLOCK_INIT,
    .lists = { [0 ... PMM_MAX_ORDER] = LIST_INIT },
};

static inline uint8_t pagecount_to_order(size_t pages) {
    if(pages == 1) return 0;
    return (uint8_t) ((sizeof(unsigned long long) * 8) - __builtin_clzll(pages - 1));
}

void pmm_region_add(uintptr_t base, size_t size, bool is_free) {
    pmm_zone_t *zones[] = { &g_pmm_zone_low, &g_pmm_zone_normal };
    for(size_t i = 0; i < sizeof(zones) / sizeof(pmm_zone_t *); i++) {
        pmm_zone_t *zone = zones[i];

        uintptr_t local_base = base;
        uintptr_t local_size = size;
        if(base + size <= zone->start || base >= zone->end) continue;
        if(local_base < zone->start) {
            local_size -= zone->start - local_base;
            local_base = zone->start;
        }
        if(local_base + local_size > zone->end) local_size = zone->end - local_base;

        size_t index_offset = local_base / ARCH_PAGE_GRANULARITY;
        size_t page_count = local_size / ARCH_PAGE_GRANULARITY;

        zone->total_page_count += page_count;

        for(size_t j = 0; j < page_count;) {
            // Approximate the order
            pmm_order_t order = pagecount_to_order(page_count - j);
            if(order > PMM_MAX_ORDER) order = PMM_MAX_ORDER;

            // Reduce it until it fits
            while(PMM_ORDER_TO_PAGECOUNT(order) > (page_count - j)) {
                ASSERT(order != 0);
                order--;
            }

            // Reduce it until it is aligned
            while(((local_base + j * ARCH_PAGE_GRANULARITY) & (PMM_ORDER_TO_PAGECOUNT(order) * ARCH_PAGE_GRANULARITY - 1)) != 0) {
                ASSERT(order != 0);
                order--;
            }

            // Initialize the block
            for(size_t y = 0; y < PMM_ORDER_TO_PAGECOUNT(order); y++) {
                page_t *page = &g_page_cache[index_offset + j + y];
                page->block.order = 0;
                page->block.max_order = order;
                page->block.free = is_free;
                if(is_free) list_push(&zone->lists[order], &page->block.list_node);
            }

            j += PMM_ORDER_TO_PAGECOUNT(order);
        }
    }
}

pmm_block_t *pmm_alloc(pmm_order_t order, pmm_flags_t flags) {
    LOG_TRACE("PMM", "alloc(oder: %u, flags: %u)", order, flags);
    ASSERT(order <= PMM_MAX_ORDER);

    pmm_order_t avl_order = order;
    pmm_zone_t *zone = (flags & PMM_FLAG_ZONE_LOW) != 0 ? &g_pmm_zone_low : &g_pmm_zone_normal;
    spinlock_acquire_nodw(&zone->lock);
    while(zone->lists[avl_order].count == 0) {
        avl_order++;
        if(avl_order > PMM_MAX_ORDER) panic("PMM", "out of memory");
    }

    pmm_block_t *block = CONTAINER_OF(list_pop(&zone->lists[avl_order]), pmm_block_t, list_node);
    for(; avl_order > order; avl_order--) {
        pmm_block_t *buddy = &PAGE(BLOCK_PADDR(block) + (PMM_ORDER_TO_PAGECOUNT(avl_order - 1) * ARCH_PAGE_GRANULARITY))->block;
        buddy->order = avl_order - 1;
        buddy->free = true;
        list_push(&zone->lists[avl_order - 1], &buddy->list_node);
    }
    spinlock_release_nodw(&zone->lock);

    block->order = order;
    block->free = false;
    zone->free_page_count -= PMM_ORDER_TO_PAGECOUNT(order);

    if((flags & PMM_FLAG_ZERO) != 0) memclear((void *) HHDM(BLOCK_PADDR(block)), PMM_ORDER_TO_PAGECOUNT(order) * ARCH_PAGE_GRANULARITY);

    LOG_TRACE("PMM", "alloc success(%#lx -> %#llx)", BLOCK_PADDR(block), BLOCK_PADDR(block) + PMM_ORDER_TO_PAGECOUNT(order) * ARCH_PAGE_GRANULARITY);

    return block;
}

pmm_block_t *pmm_alloc_pages(size_t page_count, pmm_flags_t flags) {
    return pmm_alloc(pagecount_to_order(page_count), flags);
}

pmm_block_t *pmm_alloc_page(pmm_flags_t flags) {
    return pmm_alloc(0, flags);
}

void pmm_free(pmm_block_t *block) {
    LOG_TRACE("PMM", "free(%#lx, order: %u, max_order: %u)", BLOCK_PADDR(block), block->order, block->max_order);
    pmm_zone_t *zone = (BLOCK_PADDR(block) & ~ARCH_MEM_LOW_MASK) > 0 ? &g_pmm_zone_normal : &g_pmm_zone_low;
    zone->free_page_count += PMM_ORDER_TO_PAGECOUNT(block->order);

    block->free = true;

    spinlock_acquire_nodw(&zone->lock);
    while(block->order < block->max_order) {
        pmm_block_t *buddy = &PAGE(BLOCK_PADDR(block) ^ (PMM_ORDER_TO_PAGECOUNT(block->order) * ARCH_PAGE_GRANULARITY))->block;
        if(!buddy->free || buddy->order == buddy->max_order || buddy->order != block->order) break;

        LOG_TRACE("PMM", "merging %#lx and buddy %#lx to order (%u)", BLOCK_PADDR(block), BLOCK_PADDR(buddy), block->order);

        list_node_delete(&zone->lists[block->order], &buddy->list_node);
        buddy->order++;
        block->order++;

        if(BLOCK_PADDR(buddy) < BLOCK_PADDR(block)) block = buddy;
    }
    list_push(&zone->lists[block->order], &block->list_node);
    spinlock_release_nodw(&zone->lock);
}
