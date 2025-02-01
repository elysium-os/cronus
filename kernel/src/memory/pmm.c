#include "pmm.h"

#include "arch/page.h"
#include "common/assert.h"
#include "common/panic.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "memory/hhdm.h"

uint8_t g_pmm_zone_mask = 0;
pmm_zone_t g_pmm_zones[PMM_ZONE_COUNT] = {};

static inline uint8_t pagecount_to_order(size_t pages) {
    if(pages == 1) return 0;
    return (uint8_t) ((sizeof(unsigned long long) * 8) - __builtin_clzll(pages - 1));
}

static inline size_t order_to_pagecount(uint8_t order) {
    return (size_t) 1 << order;
}

void pmm_zone_register(pmm_zone_index_t zone_index, const char *name, uintptr_t start, uintptr_t end) {
    ASSERT(!PMM_ZONE_PRESENT(zone_index));
    for(size_t i = 0; i <= PMM_ZONE_COUNT; i++) {
        if(!PMM_ZONE_PRESENT(i)) continue;
        ASSERT(start >= g_pmm_zones[i].end || end <= g_pmm_zones[i].start);
    }
    g_pmm_zone_mask |= (1 << zone_index);

    pmm_zone_t *zone = &g_pmm_zones[zone_index];
    zone->name = name;
    zone->start = start;
    zone->end = end;
    zone->lock = SPINLOCK_INIT;
    zone->regions = LIST_INIT;
    for(size_t i = 0; i <= PMM_MAX_ORDER; i++) zone->lists[i] = LIST_INIT;
}

void pmm_region_add(uintptr_t base, size_t size) {
    for(size_t i = 0; i <= PMM_ZONE_COUNT; i++) {
        pmm_zone_t *zone = &g_pmm_zones[i];
        if(!PMM_ZONE_PRESENT(i)) continue;

        uintptr_t local_base = base;
        uintptr_t local_size = size;
        if(base + size <= zone->start || base >= zone->end) continue;
        if(local_base < zone->start) {
            local_size -= zone->start - local_base;
            local_base = zone->start;
        }
        if(local_base + local_size > zone->end) local_size = zone->end - local_base;

        pmm_region_t *region = (pmm_region_t *) HHDM(local_base);
        region->zone = zone;
        region->base = local_base;
        region->page_count = local_size / ARCH_PAGE_GRANULARITY;
        region->zone->page_count += region->page_count;

        size_t used_pages = MATH_DIV_CEIL(sizeof(pmm_region_t) + sizeof(pmm_page_t) * region->page_count, ARCH_PAGE_GRANULARITY);
        region->free_count = region->page_count - used_pages;
        region->zone->free_count += region->free_count;

        for(size_t j = 0; j < region->page_count; j++) {
            region->pages[j] = (pmm_page_t) {.region = region, .free = true, .paddr = region->base + j * ARCH_PAGE_GRANULARITY};
        }

        for(size_t j = 0; j < used_pages; j++) {
            region->pages[j].free = false;
        }

        for(size_t j = used_pages, free_pages = region->free_count; free_pages;) {
            pmm_order_t order = pagecount_to_order(free_pages);
            if(free_pages & (free_pages - 1)) order--;
            if(order > PMM_MAX_ORDER) order = PMM_MAX_ORDER;

            pmm_page_t *page = &region->pages[j];
            page->order = order;
            list_append(&region->zone->lists[order], &page->list_elem);

            size_t order_size = order_to_pagecount(order);
            free_pages -= order_size;
            j += order_size;
        }

        list_append(&region->zone->regions, &region->list_elem);
    }
}

pmm_page_t *pmm_alloc(pmm_zone_index_t zone_index, pmm_order_t order, bool zero) {
    ASSERT(order <= PMM_MAX_ORDER);
    ASSERT(PMM_ZONE_PRESENT(zone_index));

    pmm_order_t avl_order = order;
    pmm_zone_t *zone = &g_pmm_zones[zone_index];
    ipl_t previous_ipl = spinlock_acquire(&zone->lock);
    while(list_is_empty(&zone->lists[avl_order])) ASSERT_COMMENT(++avl_order <= PMM_MAX_ORDER, "Out of memory");

    pmm_page_t *page = LIST_CONTAINER_GET(LIST_NEXT(&zone->lists[avl_order]), pmm_page_t, list_elem);
    list_delete(&page->list_elem);
    for(; avl_order > order; avl_order--) {
        pmm_page_t *buddy = &page->region->pages[((page->paddr - page->region->base) / ARCH_PAGE_GRANULARITY) + (order_to_pagecount(avl_order - 1))];
        buddy->order = avl_order - 1;
        list_append(&zone->lists[avl_order - 1], &buddy->list_elem);
    }
    spinlock_release(&zone->lock, previous_ipl);
    page->order = order;
    page->free = false;
    page->region->free_count -= order_to_pagecount(order);
    page->region->zone->free_count -= order_to_pagecount(order);
    if(zero) memclear((void *) HHDM(page->paddr), order_to_pagecount(order) * ARCH_PAGE_GRANULARITY);
    return page;
}

pmm_page_t *pmm_alloc_pages(pmm_zone_index_t zone_index, size_t page_count, bool zero) {
    return pmm_alloc(zone_index, pagecount_to_order(page_count), zero);
}

pmm_page_t *pmm_alloc_page(pmm_zone_index_t zone_index, bool zero) {
    return pmm_alloc(zone_index, 0, zero);
}

void pmm_free(pmm_page_t *page) {
    size_t data_base = page->region->base + MATH_CEIL(sizeof(pmm_region_t) + sizeof(pmm_page_t) * page->region->page_count, ARCH_PAGE_GRANULARITY);
    page->free = true;
    page->region->free_count += order_to_pagecount(page->order);
    page->region->zone->free_count += order_to_pagecount(page->order);
    pmm_zone_t *zone = page->region->zone;
    ipl_t previous_ipl = spinlock_acquire(&zone->lock);
    for(;;) {
        if(page->order >= PMM_MAX_ORDER) break;
        size_t buddy_addr = data_base + ((page->paddr - data_base) ^ (order_to_pagecount(page->order) * ARCH_PAGE_GRANULARITY));
        if(buddy_addr >= page->region->base + page->region->page_count * ARCH_PAGE_GRANULARITY) break;
        pmm_page_t *buddy = &page->region->pages[(buddy_addr - page->region->base) / ARCH_PAGE_GRANULARITY];
        if(!buddy->free || buddy->order != page->order) break;

        list_delete(&buddy->list_elem);
        buddy->order++;
        page->order++;
        if(buddy->paddr < page->paddr) page = buddy;
    }
    list_append(&zone->lists[page->order], &page->list_elem);
    spinlock_release(&zone->lock, previous_ipl);
}

void pmm_free_address(uintptr_t physical_address) {
    for(size_t i = 0; i <= PMM_ZONE_COUNT; i++) {
        if(!PMM_ZONE_PRESENT(i)) continue;
        pmm_zone_t *zone = &g_pmm_zones[i];
        if(zone->start > physical_address || zone->end <= physical_address) continue;
        LIST_FOREACH(&zone->regions, elem) {
            pmm_region_t *region = LIST_CONTAINER_GET(elem, pmm_region_t, list_elem);
            if(region->base > physical_address || region->base + region->page_count * ARCH_PAGE_GRANULARITY <= physical_address) continue;

            pmm_free(&region->pages[(physical_address - region->base) / ARCH_PAGE_GRANULARITY]);
        }
    }
}
