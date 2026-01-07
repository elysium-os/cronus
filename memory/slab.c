#include "memory/slab.h"

#include "arch/cpu.h"
#include "arch/page.h"
#include "common/assert.h"
#include "memory/hhdm.h"
#include "memory/page.h"
#include "sys/hook.h"
#include "sys/init.h"

#define MAGAZINE_SIZE 32
#define MAGAZINE_COUNT_EXTRA (g_cpu_count * 2)

static spinlock_t g_slab_caches_lock = SPINLOCK_INIT;
static list_t g_slab_caches = LIST_INIT;

static slab_cache_t g_alloc_cache;
static slab_cache_t g_alloc_magazine;

// TODO free slabs...

static slab_t *cache_make_slab(slab_cache_t *cache) {
    pmm_block_t *block = pmm_alloc(cache->block_order, PMM_FLAG_NONE);

    slab_t *slab = (slab_t *) HHDM(PAGE_PADDR(PAGE_FROM_BLOCK(block)));
    slab->cache = cache;
    slab->block = block;
    slab->freelist = nullptr;
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
    spinlock_acquire_nodw(&cache->slabs_lock);

    if(cache->slabs_partial.count == 0) list_push(&cache->slabs_partial, &cache_make_slab(cache)->list_node);
    slab_t *slab = CONTAINER_OF(cache->slabs_partial.head, slab_t, list_node);
    ASSERT(slab->free_count > 0);

    void *obj = slab->freelist;
    slab->freelist = *(void **) obj;
    slab->free_count--;
    if(slab->free_count == 0) {
        list_node_delete(&cache->slabs_partial, &slab->list_node);
        list_push(&cache->slabs_full, &slab->list_node);
    }

    spinlock_release_nodw(&cache->slabs_lock);
    return obj;
}

static void slab_direct_free(slab_cache_t *cache, void *obj) {
    spinlock_acquire_nodw(&cache->slabs_lock);

    slab_t *slab = (slab_t *) (((uintptr_t) obj) & ~(PMM_ORDER_TO_PAGECOUNT(cache->block_order) * ARCH_PAGE_GRANULARITY - 1));
    *(void **) obj = slab->freelist;
    slab->freelist = obj;
    if(slab->free_count == 0) {
        list_node_delete(&cache->slabs_full, &slab->list_node);
        list_push(&cache->slabs_partial, &slab->list_node);
    }
    slab->free_count++;

    spinlock_release_nodw(&cache->slabs_lock);
}

slab_cache_t *slab_cache_create(const char *name, size_t object_size, pmm_order_t order) {
    ASSERT(object_size >= 8);

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
        for(size_t j = 0; j < MAGAZINE_SIZE; j++) magazine->rounds[j] = nullptr;
        list_push(&cache->magazines_empty, &magazine->list_node);
    }

    if(cache->cpu_cache_enabled) {
        for(size_t i = 0; i < g_cpu_count; i++) {
            slab_magazine_t *magazine_primary = slab_allocate(&g_alloc_magazine);
            magazine_primary->round_count = MAGAZINE_SIZE;
            for(size_t j = 0; j < MAGAZINE_SIZE; j++) magazine_primary->rounds[j] = slab_direct_alloc(cache);

            slab_magazine_t *magazine_secondary = slab_allocate(&g_alloc_magazine);
            magazine_secondary->round_count = 0;
            for(size_t j = 0; j < MAGAZINE_SIZE; j++) magazine_secondary->rounds[j] = nullptr;

            cache->cpu_cache[i].lock = SPINLOCK_INIT;
            cache->cpu_cache[i].primary = magazine_primary;
            cache->cpu_cache[i].secondary = magazine_secondary;
        }
    }

    spinlock_acquire_nodw(&g_slab_caches_lock);
    list_push(&g_slab_caches, &cache->list_node);
    spinlock_release_nodw(&g_slab_caches_lock);

    return cache;
}

void *slab_allocate(slab_cache_t *cache) {
    if(!cache->cpu_cache_enabled) return slab_direct_alloc(cache);

    sched_preempt_inc();
    slab_cache_cpu_t *cc = &cache->cpu_cache[ARCH_CPU_CURRENT_READ(sequential_id)];
    spinlock_acquire_nodw(&cc->lock);
    sched_preempt_dec();

alloc:
    if(cc->primary->round_count > 0) {
        void *obj = cc->primary->rounds[--cc->primary->round_count];
        spinlock_release_nodw(&cc->lock);
        return obj;
    }

    if(cc->secondary->round_count == MAGAZINE_SIZE) {
        slab_magazine_t *mag = cc->primary;
        cc->primary = cc->secondary;
        cc->secondary = mag;
        goto alloc;
    }

    spinlock_acquire_raw(&cache->magazines_lock);
    if(cache->magazines_full.count != 0) {
        list_push(&cache->magazines_empty, &cc->secondary->list_node);
        cc->secondary = cc->primary;
        cc->primary = CONTAINER_OF(list_pop(&cache->magazines_full), slab_magazine_t, list_node);

        spinlock_release_raw(&cache->magazines_lock);
        goto alloc;
    }
    spinlock_release_raw(&cache->magazines_lock);

    spinlock_release_nodw(&cc->lock);
    return slab_direct_alloc(cache);
}

void slab_free(slab_cache_t *cache, void *obj) {
    if(!cache->cpu_cache_enabled) return slab_direct_free(cache, obj);

    sched_preempt_inc();
    slab_cache_cpu_t *cc = &cache->cpu_cache[ARCH_CPU_CURRENT_READ(sequential_id)];
    spinlock_acquire_nodw(&cc->lock);
    sched_preempt_dec();

free:
    if(cc->primary->round_count < MAGAZINE_SIZE) {
        cc->primary->rounds[cc->primary->round_count++] = obj;
        spinlock_release_nodw(&cc->lock);
        return;
    }

    if(cc->secondary->round_count == 0) {
        slab_magazine_t *mag = cc->primary;
        cc->primary = cc->secondary;
        cc->secondary = mag;
        goto free;
    }

    spinlock_acquire_raw(&cache->magazines_lock);
    if(cache->magazines_empty.count != 0) {
        list_push(&cache->magazines_full, &cc->secondary->list_node);
        cc->secondary = cc->primary;
        cc->primary = CONTAINER_OF(list_pop(&cache->magazines_empty), slab_magazine_t, list_node);

        spinlock_release_raw(&cache->magazines_lock);
        goto free;
    }
    spinlock_release_raw(&cache->magazines_lock);

    spinlock_release_nodw(&cc->lock);
    slab_direct_free(cache, obj);
}

INIT_TARGET(slab, INIT_STAGE_BEFORE_MAIN, INIT_SCOPE_BSP, INIT_DEPS()) {
    g_alloc_cache = (slab_cache_t) {
        .name = "slab-cache",
        .object_size = sizeof(slab_cache_t) + g_cpu_count * sizeof(slab_cache_cpu_t),
        .block_order = 3,
        .slabs_lock = SPINLOCK_INIT,
        .slabs_full = LIST_INIT,
        .slabs_partial = LIST_INIT,
        .magazines_lock = SPINLOCK_INIT,
        .magazines_full = LIST_INIT,
        .magazines_empty = LIST_INIT,
        .cpu_cache_enabled = false,
    };
    g_alloc_magazine = (slab_cache_t) {
        .name = "slab-magazine",
        .object_size = sizeof(slab_magazine_t) + MAGAZINE_SIZE * sizeof(void *),
        .block_order = 2,
        .slabs_lock = SPINLOCK_INIT,
        .slabs_full = LIST_INIT,
        .slabs_partial = LIST_INIT,
        .magazines_lock = SPINLOCK_INIT,
        .magazines_full = LIST_INIT,
        .magazines_empty = LIST_INIT,
        .cpu_cache_enabled = false,
    };
    list_push(&g_slab_caches, &g_alloc_cache.list_node);
    list_push(&g_slab_caches, &g_alloc_magazine.list_node);

    HOOK_RUN(init_slab_cache);
}
