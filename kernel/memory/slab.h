#pragma once

#include "common/lock/spinlock.h"
#include "lib/list.h"
#include "memory/pmm.h"

#include <stddef.h>

typedef struct {
    list_node_t list_node;
    size_t round_count;
    void *rounds[];
} slab_magazine_t;

typedef struct {
    spinlock_t lock;
    slab_magazine_t *primary, *secondary;
} slab_cache_cpu_t;

typedef struct {
    const char *name;
    size_t object_size;
    pmm_order_t block_order;

    list_node_t list_node;

    spinlock_t slabs_lock;
    list_t slabs_full;
    list_t slabs_partial;

    spinlock_t magazines_lock;
    list_t magazines_full;
    list_t magazines_empty;

    bool cpu_cache_enabled;
    slab_cache_cpu_t cpu_cache[];
} slab_cache_t;

typedef struct {
    slab_cache_t *cache;
    list_node_t list_node;
    pmm_block_t *block;

    size_t free_count;
    void *freelist;
} slab_t;

/// Create slab cache.
/// @param order The block order of each slab in the cache
slab_cache_t *slab_cache_create(const char *name, size_t object_size, pmm_order_t order);

/// Allocate an object from a cache.
void *slab_allocate(slab_cache_t *cache);

/// Free a previously allocated object to its cache.
void slab_free(slab_cache_t *cache, void *obj);
