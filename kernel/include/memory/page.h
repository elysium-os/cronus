#pragma once

#include "arch/page.h"
#include "lib/container.h"
#include "memory/pmm.h"

#define PAGE(PHYSICAL_ADDRESS) (&(g_page_cache[(PHYSICAL_ADDRESS) / PAGE_GRANULARITY]))
#define PAGE_PADDR(PAGE) (((uintptr_t) (PAGE) - (uintptr_t) g_page_cache) / sizeof(page_t) * PAGE_GRANULARITY)

#define PAGE_FROM_BLOCK(BLOCK) (CONTAINER_OF((BLOCK), page_t, block))

typedef struct {
    pmm_block_t block;
} page_t;

extern page_t *g_page_cache;
extern size_t g_page_cache_size;
