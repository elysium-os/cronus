#include "heap.h"

#include "arch/page.h"
#include "common/assert.h"
#include "lib/expect.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "memory/hhdm.h"
#include "memory/page.h"
#include "memory/pmm.h"
#include "memory/slab.h"

#define SLAB_8X_COUNT (sizeof(g_slab_8x_sizes) / sizeof(*g_slab_8x_sizes))
#define SLAB_128X_COUNT (sizeof(g_slab_128x_sizes) / sizeof(*g_slab_128x_sizes))
#define SLAB_OTHER_COUNT (sizeof(g_slab_other_sizes) / sizeof(*g_slab_other_sizes))

static const char *g_slab_8x_names[] = {"heap-8", "heap-16", "heap-24", "heap-32", "heap-40", "heap-48", "heap-56", "heap-64", "heap-72"};
static size_t g_slab_8x_sizes[] = {8, 16, 24, 32, 40, 48, 56, 64, 72};
static slab_cache_t *g_8x_slabs[SLAB_8X_COUNT];

static const char *g_slab_128x_names[] = {"heap-128", "heap-256", "heap-384", "heap-512"};
static size_t g_slab_128x_sizes[] = {128, 256, 384, 512};
static slab_cache_t *g_128x_slabs[SLAB_128X_COUNT];

static const char *g_slab_other_names[] = {"heap-1024", "heap-2048"};
static size_t g_slab_other_sizes[] = {1024, 2048};
static slab_cache_t *g_other_slabs[SLAB_OTHER_COUNT];

static slab_cache_t *find_cache(size_t size) {
    slab_cache_t *cache = NULL;
    if(EXPECT_LIKELY(size <= g_slab_8x_sizes[SLAB_8X_COUNT - 1])) {
        cache = g_8x_slabs[MATH_DIV_CEIL(size, 8) - 1];
    } else if(EXPECT_LIKELY(size <= g_slab_128x_sizes[SLAB_128X_COUNT - 1])) {
        cache = g_128x_slabs[MATH_DIV_CEIL(size, 128) - 1];
    } else {
        for(size_t i = 0; i < SLAB_OTHER_COUNT; i++) {
            if(g_slab_other_sizes[i] < size) continue;
            cache = g_other_slabs[i];
            break;
        }
    }
    ASSERT(cache != NULL);
    return cache;
}

void heap_initialize() {
    for(size_t i = 0; i < SLAB_8X_COUNT; i++) g_8x_slabs[i] = slab_cache_create(g_slab_8x_names[i], g_slab_8x_sizes[i], 2);
    for(size_t i = 0; i < SLAB_128X_COUNT; i++) g_128x_slabs[i] = slab_cache_create(g_slab_128x_names[i], g_slab_128x_sizes[i], 3);
    for(size_t i = 0; i < SLAB_OTHER_COUNT; i++) g_other_slabs[i] = slab_cache_create(g_slab_other_names[i], g_slab_other_sizes[i], 5);
}

void *heap_alloc(size_t size) {
    if(size == 0) return NULL;
    if(EXPECT_UNLIKELY(size > g_slab_other_sizes[SLAB_OTHER_COUNT - 1]))
        return (void *) HHDM(pmm_alloc_pages(MATH_DIV_CEIL(size, ARCH_PAGE_GRANULARITY), PMM_FLAG_NONE)->paddr);
    return slab_allocate(find_cache(size));
}

void *heap_realloc(void *address, size_t current_size, size_t new_size) {
    if(current_size == new_size) return address;
    if(address == NULL && new_size == 0) return address;

    void *new_address = heap_alloc(new_size);
    if(address == NULL || current_size == 0) return new_address;

    memcpy(new_address, address, current_size > new_size ? current_size : new_size);
    heap_free(address, current_size);
    return new_address;
}

void *heap_reallocarray(void *array, size_t element_size, size_t current_count, size_t new_count) {
    return heap_realloc(array, current_count * element_size, new_count * element_size);
}

void heap_free(void *address, size_t size) {
    if(address == NULL) return;
    if(size > g_slab_other_sizes[SLAB_OTHER_COUNT - 1]) return pmm_free(&PAGE(HHDM_TO_PHYS(address))->block);
    slab_free(find_cache(size), address);
}
