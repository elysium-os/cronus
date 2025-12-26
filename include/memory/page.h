#pragma once

#include "arch/page.h"
#include "lib/container.h"
#include "memory/pmm.h"

#define PAGE(PHYSICAL_ADDRESS) (&(g_page_db[(PHYSICAL_ADDRESS) / ARCH_PAGE_GRANULARITY]))
#define PAGE_PADDR(PAGE) (((uintptr_t) (PAGE) - (uintptr_t) g_page_db) / sizeof(page_t) * ARCH_PAGE_GRANULARITY)

#define PAGE_FROM_BLOCK(BLOCK) (CONTAINER_OF((BLOCK), page_t, block))

typedef struct {
    pmm_block_t block;
} page_t;

extern page_t *g_page_db;
extern size_t g_page_db_size;
