#include "slab.h"

#include "arch/cpu.h"
#include "arch/page.h"
#include "common/assert.h"
#include "memory/hhdm.h"

#define MAGAZINE_SIZE 32
#define MAGAZINE_COUNT_EXTRA arch_cpu_count() * 2

static spinlock_t g_slab_caches_lock = SPINLOCK_INIT;
static list_t g_slab_caches = LIST_INIT;

static slab_cache_t g_alloc_cache;
static slab_cache_t g_alloc_magazine;

// TODO free slabs...

static slab_t *cache_make_slab(slab_cache_t *cache) {
    pmm_block_t *block = pmm_alloc(cache->block_order, PMM_FLAG_NONE);

    slab_t *slab = (slab_t *) HHDM(block->paddr);
    slab->cache = cache;
    slab->block = block;
    slab->freelist = NULL;
    slab->free_count = 0;

    size_t slab_size = PMM_ORDER_TO_PAGECOUNT(cache->block_order) * ARCH_PAGE_GRANULARITY - sizeof(slab_t);
    ASSERT(slab_size / cache->object_size > 0);
    for(size_t i = 0; i < slab_size / cache->object_size; i++) {
        void **obj = (void **) (((uintptr_t) slab) + sizeof(slab_t) + (i * cache->object_size));
        *obj = slab->freelist;
        slab->freelist = obj;
        slab->free_count++;
    }
    return slab;
}

static void *slab_direct_alloc(slab_cache_t *cache) {
    interrupt_state_t previous_state = spinlock_acquire(&cache->slabs_lock);

    if(list_is_empty(&cache->slabs_partial)) list_append(&cache->slabs_partial, &cache_make_slab(cache)->list_elem);
    slab_t *slab = LIST_CONTAINER_GET(LIST_NEXT(&cache->slabs_partial), slab_t, list_elem);
    ASSERT(slab->free_count > 0);

    void *obj = slab->freelist;
    slab->freelist = *(void **) obj;
    slab->free_count--;
    if(slab->free_count == 0) {
        list_delete(&slab->list_elem);
        list_append(&cache->slabs_full, &slab->list_elem);
    }

    spinlock_release(&cache->slabs_lock, previous_state);
    return obj;
}

static void slab_direct_free(slab_cache_t *cache, void *obj) {
    interrupt_state_t previous_state = spinlock_acquire(&cache->slabs_lock);

    slab_t *slab = (slab_t *) (((uintptr_t) obj) & ~(PMM_ORDER_TO_PAGECOUNT(cache->block_order) * ARCH_PAGE_GRANULARITY - 1));
    *(void **) obj = slab->freelist;
    slab->freelist = obj;
    if(slab->free_count == 0) {
        list_delete(&slab->list_elem);
        list_append(&cache->slabs_partial, &slab->list_elem);
    }
    slab->free_count++;

    spinlock_release(&cache->slabs_lock, previous_state);
}

void slab_init() {
    g_alloc_cache = (slab_cache_t) {.name = "slab-cache",
                                    .object_size = sizeof(slab_cache_t) + arch_cpu_count() * sizeof(slab_cache_cpu_t),
                                    .block_order = 3,
                                    .slabs_lock = SPINLOCK_INIT,
                                    .slabs_full = LIST_INIT,
                                    .slabs_partial = LIST_INIT,
                                    .magazines_lock = SPINLOCK_INIT,
                                    .magazines_full = LIST_INIT,
                                    .magazines_empty = LIST_INIT,
                                    .cpu_cache_enabled = false};
    g_alloc_magazine = (slab_cache_t) {.name = "slab-magazine",
                                       .object_size = sizeof(slab_magazine_t) + MAGAZINE_SIZE * sizeof(void *),
                                       .block_order = 2,
                                       .slabs_lock = SPINLOCK_INIT,
                                       .slabs_full = LIST_INIT,
                                       .slabs_partial = LIST_INIT,
                                       .magazines_lock = SPINLOCK_INIT,
                                       .magazines_full = LIST_INIT,
                                       .magazines_empty = LIST_INIT,
                                       .cpu_cache_enabled = false};
    list_append(&g_slab_caches, &g_alloc_cache.list_elem);
    list_append(&g_slab_caches, &g_alloc_magazine.list_elem);
}

slab_cache_t *slab_cache_create(const char *name, size_t object_size, pmm_order_t order) {
    slab_cache_t *cache = slab_allocate(&g_alloc_cache);
    cache->name = name;
    cache->object_size = object_size;
    cache->block_order = order;
    cache->cpu_cache_enabled = true;

    cache->slabs_lock = SPINLOCK_INIT;
    cache->slabs_full = LIST_INIT;
    cache->slabs_partial = LIST_INIT;

    cache->magazines_lock = SPINLOCK_INIT;
    cache->magazines_full = LIST_INIT;
    cache->magazines_empty = LIST_INIT;

    for(size_t i = 0; i < MAGAZINE_COUNT_EXTRA; i++) {
        slab_magazine_t *magazine = slab_allocate(&g_alloc_magazine);
        magazine->round_count = 0;
        for(size_t j = 0; j < MAGAZINE_SIZE; j++) magazine->rounds[j] = NULL;
        list_append(&cache->magazines_empty, &magazine->list_elem);
    }

    if(cache->cpu_cache_enabled) {
        for(size_t i = 0; i < arch_cpu_count(); i++) {
            slab_magazine_t *magazine_primary = slab_allocate(&g_alloc_magazine);
            magazine_primary->round_count = MAGAZINE_SIZE;
            for(size_t j = 0; j < MAGAZINE_SIZE; j++) magazine_primary->rounds[j] = slab_direct_alloc(cache);

            slab_magazine_t *magazine_secondary = slab_allocate(&g_alloc_magazine);
            magazine_secondary->round_count = 0;
            for(size_t j = 0; j < MAGAZINE_SIZE; j++) magazine_secondary->rounds[j] = NULL;

            cache->cpu_cache[i].lock = SPINLOCK_INIT;
            cache->cpu_cache[i].primary = magazine_primary;
            cache->cpu_cache[i].secondary = magazine_secondary;
        }
    }

    interrupt_state_t previous_state = spinlock_acquire(&g_slab_caches_lock);
    list_append(&g_slab_caches, &cache->list_elem);
    spinlock_release(&g_slab_caches_lock, previous_state);

    return cache;
}

void *slab_allocate(slab_cache_t *cache) {
    if(!cache->cpu_cache_enabled) return slab_direct_alloc(cache);

    slab_cache_cpu_t *cc = &cache->cpu_cache[arch_cpu_id()];
    interrupt_state_t previous_state = spinlock_acquire(&cc->lock);

alloc:
    if(cc->primary->round_count > 0) {
        void *obj = cc->primary->rounds[--cc->primary->round_count];
        spinlock_release(&cc->lock, previous_state);
        return obj;
    }

    if(cc->secondary->round_count == MAGAZINE_SIZE) {
        slab_magazine_t *mag = cc->primary;
        cc->primary = cc->secondary;
        cc->secondary = mag;
        goto alloc;
    }

    spinlock_primitive_acquire(&cache->magazines_lock);
    if(!list_is_empty(&cache->magazines_full)) {
        list_append(&cache->magazines_empty, &cc->secondary->list_elem);
        cc->secondary = cc->primary;

        slab_magazine_t *new = LIST_CONTAINER_GET(LIST_NEXT(&cache->magazines_full), slab_magazine_t, list_elem);
        list_delete(&new->list_elem);
        cc->primary = new;

        spinlock_primitive_release(&cache->magazines_lock);
        goto alloc;
    }
    spinlock_primitive_release(&cache->magazines_lock);

    spinlock_release(&cc->lock, previous_state);
    return slab_direct_alloc(cache);
}

void slab_free(slab_cache_t *cache, void *obj) {
    if(!cache->cpu_cache_enabled) return slab_direct_free(cache, obj);

    slab_cache_cpu_t *cc = &cache->cpu_cache[arch_cpu_id()];
    interrupt_state_t previous_state = spinlock_acquire(&cc->lock);

free:
    if(cc->primary->round_count < MAGAZINE_SIZE) {
        cc->primary->rounds[cc->primary->round_count++] = obj;
        spinlock_release(&cc->lock, previous_state);
        return;
    }

    if(cc->secondary->round_count == 0) {
        slab_magazine_t *mag = cc->primary;
        cc->primary = cc->secondary;
        cc->secondary = mag;
        goto free;
    }

    spinlock_primitive_acquire(&cache->magazines_lock);
    if(!list_is_empty(&cache->magazines_empty)) {
        list_append(&cache->magazines_full, &cc->secondary->list_elem);
        cc->secondary = cc->primary;

        slab_magazine_t *new = LIST_CONTAINER_GET(LIST_NEXT(&cache->magazines_empty), slab_magazine_t, list_elem);
        list_delete(&new->list_elem);
        cc->primary = new;

        spinlock_primitive_release(&cache->magazines_lock);
        goto free;
    }
    spinlock_primitive_release(&cache->magazines_lock);

    spinlock_release(&cc->lock, previous_state);
    slab_direct_free(cache, obj);
}
