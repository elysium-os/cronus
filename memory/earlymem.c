#include "memory/earlymem.h"

#include "arch/page.h"
#include "common/assert.h"
#include "common/panic.h"
#include "init.h"
#include "lib/container.h"
#include "lib/list.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "memory/hhdm.h"
#include "memory/page.h"
#include "memory/pmm.h"
#include "sys/init.h"

#define BITMAP(REGION) ((uint8_t *) ((uintptr_t) (REGION) + sizeof(earlymem_region_t)))
#define BITMAP_SIZE(REGION) ((REGION)->length / ARCH_PAGE_GRANULARITY / 8)

typedef struct {
    uintptr_t base;
    size_t length;
    size_t hint;
    list_node_t list_node;
} earlymem_region_t;

bool g_earlymem_active = true;

static list_t g_earlymem_regions = LIST_INIT;
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

static void region_add(uintptr_t address, size_t length) {
    earlymem_region_t *region = (void *) HHDM(address);
    region->base = address;
    region->length = length;
    region->hint = 0;
    mem_set(BITMAP(region), 0, BITMAP_SIZE(region));
    for(size_t i = 0; i < MATH_DIV_CEIL(BITMAP_SIZE(region), ARCH_PAGE_GRANULARITY); i++) bitmap_set(region, i, true);
    list_push(&g_earlymem_regions, &region->list_node);
}

static bool region_isfree(earlymem_region_t *region, size_t offset) {
    ASSERT(offset < region->length);
    return !bitmap_get(region, offset / ARCH_PAGE_GRANULARITY);
}

uintptr_t earlymem_alloc_page() {
    LIST_ITERATE(&g_earlymem_regions, node) {
        earlymem_region_t *region = CONTAINER_OF(node, earlymem_region_t, list_node);

        if(region->hint == region->length / ARCH_PAGE_GRANULARITY) continue;

        for(size_t i = region->hint; i < region->length / ARCH_PAGE_GRANULARITY; i++) {
            region->hint = i;
            if(bitmap_get(region, i)) continue;
            bitmap_set(region, i, true);
            return region->base + i * ARCH_PAGE_GRANULARITY;
        }
    }

    LIST_ITERATE(&g_earlymem_regions, node) {
        earlymem_region_t *region = CONTAINER_OF(node, earlymem_region_t, list_node);
        for(size_t i = 0; i < region->length / ARCH_PAGE_GRANULARITY; i++) {
            region->hint = i;
            if(bitmap_get(region, i)) continue;
            bitmap_set(region, i, true);
            return region->base + i * ARCH_PAGE_GRANULARITY;
        }
    }

    panic("EARLY_MM", "out of memory");
}

void earlymem_free_page(uintptr_t address) {
    LIST_ITERATE(&g_earlymem_regions, node) {
        earlymem_region_t *region = CONTAINER_OF(node, earlymem_region_t, list_node);

        if(address < region->base || address >= region->base + region->length) continue;

        bitmap_set(region, (address - region->base) / ARCH_PAGE_GRANULARITY, false);
    }
}

uintptr_t earlymem_alloc(size_t size) {
    ASSERT(size > 0 && size <= ARCH_PAGE_GRANULARITY);
    if(g_alloc_offset == 0 || ARCH_PAGE_GRANULARITY - g_alloc_offset < size) {
        g_alloc_page = earlymem_alloc_page();
        g_alloc_offset = 0;
    }
    size_t offset = g_alloc_offset;
    g_alloc_offset += size;
    return g_alloc_page + offset;
}

INIT_TARGET(early_mem, INIT_PROVIDES("earlymem"), INIT_DEPS("hhdm", "assert")) {
    for(size_t i = 0; i < g_init_boot_info->mm_entry_count; i++) {
        tartarus_mm_entry_t *entry = &g_init_boot_info->mm_entries[i];

        for(size_t j = i + 1; j < g_init_boot_info->mm_entry_count; j++) {
            ASSERT((entry->base >= (g_init_boot_info->mm_entries[j].base + g_init_boot_info->mm_entries[j].length)) || (g_init_boot_info->mm_entries[j].base >= (entry->base + entry->length)));
        }

        switch(entry->type) {
            case TARTARUS_MM_TYPE_USABLE: break;

            case TARTARUS_MM_TYPE_BOOTLOADER_RECLAIMABLE:
            case TARTARUS_MM_TYPE_EFI_RECLAIMABLE:
            case TARTARUS_MM_TYPE_ACPI_RECLAIMABLE:
            case TARTARUS_MM_TYPE_ACPI_NVS:
            case TARTARUS_MM_TYPE_RESERVED:
            case TARTARUS_MM_TYPE_BAD:                    continue;
        }

        ASSERT(entry->base % ARCH_PAGE_GRANULARITY == 0 && entry->length % ARCH_PAGE_GRANULARITY == 0);

        region_add(entry->base, entry->length); // TODO: coalesce these regions so that we dont end up with weird max_orders in the buddy
    }
}

INIT_TARGET(earlymem_handover, INIT_PROVIDES("pmm", "memory"), INIT_DEPS("pmm_regions")) {
    // TODO: release reclaimable regions into pmm as well as
    //       merging with free ones for max order to settle properly.
    LIST_ITERATE(&g_earlymem_regions, node) {
        earlymem_region_t *region = CONTAINER_OF(node, earlymem_region_t, list_node);
        for(size_t offset = 0; offset < region->length; offset += ARCH_PAGE_GRANULARITY) {
            if(!region_isfree(region, offset)) continue;
            pmm_free(&PAGE(region->base + offset)->block);
        }
    }
}
