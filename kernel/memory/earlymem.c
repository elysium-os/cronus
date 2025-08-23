#include "memory/earlymem.h"

#include "arch/page.h"
#include "common/assert.h"
#include "common/panic.h"
#include "lib/container.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "memory/hhdm.h"

#define BITMAP(REGION) ((uint8_t *) ((uintptr_t) (REGION) + sizeof(earlymem_region_t)))
#define BITMAP_SIZE(REGION) ((REGION)->length / PAGE_GRANULARITY / 8)

bool g_earlymem_active = true;
list_t g_earlymem_regions = LIST_INIT;

static uintptr_t g_alloc_page = 0;
static size_t g_alloc_offset = 0;

static void bitmap_set(earlymem_region_t *region, size_t i, bool value) {
    uint8_t mask = 1 << (i % 8);
    if(value) {
        BITMAP(region)[i / 8] |= mask;
    } else {
        BITMAP(region)[i / 8] &= ~mask;
    }
}

static bool bitmap_get(earlymem_region_t *region, size_t i) {
    return (BITMAP(region)[i / 8] & (1 << (i % 8))) != 0;
}

void earlymem_region_add(uintptr_t address, size_t length) {
    earlymem_region_t *region = (void *) HHDM(address);
    region->base = address;
    region->length = length;
    region->hint = 0;
    memset(BITMAP(region), 0, BITMAP_SIZE(region));
    for(size_t i = 0; i < MATH_DIV_CEIL(BITMAP_SIZE(region), PAGE_GRANULARITY); i++) bitmap_set(region, i, true);
    list_push(&g_earlymem_regions, &region->list_node);
}

bool earlymem_region_isfree(earlymem_region_t *region, size_t offset) {
    ASSERT(offset < region->length);
    return !bitmap_get(region, offset / PAGE_GRANULARITY);
}

uintptr_t earlymem_alloc_page() {
    LIST_ITERATE(&g_earlymem_regions, node) {
        earlymem_region_t *region = CONTAINER_OF(node, earlymem_region_t, list_node);

        if(region->hint == region->length / PAGE_GRANULARITY) continue;

        for(size_t i = region->hint; i < region->length / PAGE_GRANULARITY; i++) {
            region->hint = i;
            if(bitmap_get(region, i)) continue;
            bitmap_set(region, i, true);
            return region->base + i * PAGE_GRANULARITY;
        }
    }

    LIST_ITERATE(&g_earlymem_regions, node) {
        earlymem_region_t *region = CONTAINER_OF(node, earlymem_region_t, list_node);
        for(size_t i = 0; i < region->length / PAGE_GRANULARITY; i++) {
            region->hint = i;
            if(bitmap_get(region, i)) continue;
            bitmap_set(region, i, true);
            return region->base + i * PAGE_GRANULARITY;
        }
    }

    panic("EARLY_MM", "out of memory");
}

void earlymem_free_page(uintptr_t address) {
    LIST_ITERATE(&g_earlymem_regions, node) {
        earlymem_region_t *region = CONTAINER_OF(node, earlymem_region_t, list_node);

        if(address < region->base || address >= region->base + region->length) continue;

        bitmap_set(region, (address - region->base) / PAGE_GRANULARITY, false);
    }
}

uintptr_t earlymem_alloc(size_t size) {
    ASSERT(size > 0 && size <= PAGE_GRANULARITY);
    if(g_alloc_offset == 0 || PAGE_GRANULARITY - g_alloc_offset < size) {
        g_alloc_page = earlymem_alloc_page();
        g_alloc_offset = 0;
    }
    size_t offset = g_alloc_offset;
    g_alloc_offset += size;
    return g_alloc_page + offset;
}
