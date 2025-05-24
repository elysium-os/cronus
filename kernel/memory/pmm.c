#include "pmm.h"

#include "arch/mem.h"
#include "arch/page.h"
#include "common/assert.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "memory/hhdm.h"
#include "memory/page.h"

#include <stdint.h>

#define BLOCK_PADDR(BLOCK) PAGE_PADDR(PAGE_FROM_BLOCK(BLOCK))

pmm_zone_t g_pmm_zone_low = { .name = "LOW", .start = ARCH_PAGE_GRANULARITY, .end = ARCH_MEM_LOW_SIZE, .total_page_count = 0, .free_page_count = 0, .lock = SPINLOCK_INIT, .lists = { [0 ... PMM_MAX_ORDER] = LIST_INIT } };
pmm_zone_t
    g_pmm_zone_normal = { .name = "NORMAL", .start = ARCH_MEM_LOW_SIZE, .end = MATH_FLOOR(UINTPTR_MAX, ARCH_PAGE_GRANULARITY), .total_page_count = 0, .free_page_count = 0, .lock = SPINLOCK_INIT, .lists = { [0 ... PMM_MAX_ORDER] = LIST_INIT } };

static inline uint8_t pagecount_to_order(size_t pages) {
    if(pages == 1) return 0;
    return (uint8_t) ((sizeof(unsigned long long) * 8) - __builtin_clzll(pages - 1));
}

void pmm_region_add(uintptr_t base, size_t size, size_t used) {
    ASSERT(base >= ARCH_PAGE_GRANULARITY);
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
        zone->free_page_count += page_count - used;

        for(size_t j = 0; j < page_count; j++) {
            g_page_cache[index_offset + j].block = (pmm_block_t) { .free = true, .order = 0 };
        }

        for(size_t j = 0; j < used; j++) {
            g_page_cache[index_offset + j].block.free = false;
        }

        for(size_t j = used; j < page_count;) {
            pmm_order_t order = pagecount_to_order(page_count - j);
            if(order > PMM_MAX_ORDER) order = PMM_MAX_ORDER;
            while(PMM_ORDER_TO_PAGECOUNT(order) > (page_count - j) || ((local_base + j * ARCH_PAGE_GRANULARITY) & (PMM_ORDER_TO_PAGECOUNT(order) * ARCH_PAGE_GRANULARITY - 1)) != 0) {
                ASSERT(order != 0);
                order--;
            }

            page_t *page = &g_page_cache[index_offset + j];
            page->block.order = order;
            page->block.max_order = order;
            list_push(&zone->lists[order], &page->block.list_node);

            j += PMM_ORDER_TO_PAGECOUNT(order);
        }

        if(used > page_count) {
            used -= page_count;
        } else {
            used = 0;
        }
    }
}

pmm_block_t *pmm_alloc(pmm_order_t order, pmm_flags_t flags) {
    ASSERT(order <= PMM_MAX_ORDER);

    pmm_order_t avl_order = order;
    pmm_zone_t *zone = (flags & PMM_FLAG_ZONE_LOW) != 0 ? &g_pmm_zone_low : &g_pmm_zone_normal;
    interrupt_state_t previous_state = spinlock_acquire(&zone->lock);
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
    spinlock_release(&zone->lock, previous_state);

    block->order = order;
    block->free = false;
    zone->free_page_count -= PMM_ORDER_TO_PAGECOUNT(order);

    if((flags & PMM_FLAG_ZERO) != 0) memclear((void *) HHDM(BLOCK_PADDR(block)), PMM_ORDER_TO_PAGECOUNT(order) * ARCH_PAGE_GRANULARITY);

    return block;
}

pmm_block_t *pmm_alloc_pages(size_t page_count, pmm_flags_t flags) {
    return pmm_alloc(pagecount_to_order(page_count), flags);
}

pmm_block_t *pmm_alloc_page(pmm_flags_t flags) {
    return pmm_alloc(0, flags);
}

void pmm_free(pmm_block_t *block) {
    pmm_zone_t *zone = (BLOCK_PADDR(block) & ~ARCH_MEM_LOW_MASK) > 0 ? &g_pmm_zone_normal : &g_pmm_zone_low;
    zone->free_page_count += PMM_ORDER_TO_PAGECOUNT(block->order);

    block->free = true;

    interrupt_state_t previous_state = spinlock_acquire(&zone->lock);
    while(block->order < block->max_order) {
        pmm_block_t *buddy = &PAGE(BLOCK_PADDR(block) ^ (PMM_ORDER_TO_PAGECOUNT(block->order) * ARCH_PAGE_GRANULARITY))->block;
        if(!buddy->free || buddy->order != block->order) break;

        list_node_delete(&zone->lists[block->order], &buddy->list_node);
        buddy->order++;
        block->order++;

        if(BLOCK_PADDR(buddy) < BLOCK_PADDR(block)) block = buddy;
    }
    list_push(&zone->lists[block->order], &block->list_node);
    spinlock_release(&zone->lock, previous_state);
}
