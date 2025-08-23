#include "arch/page.h"
#include "arch/ptm.h"
#include "common/assert.h"
#include "common/log.h"
#include "memory/hhdm.h"
#include "memory/page.h"
#include "memory/pmm.h"

void __module_initialize() {
    log(LOG_LEVEL_INFO, "TEST_PMM", "Running PMM tests");

    pmm_block_t *block = pmm_alloc(0, PMM_FLAG_NONE);
    ASSERT(block->order == 0);
    ASSERT(block->free == false);

    pmm_free(block);
    ASSERT(block->free == true);

    pmm_block_t *zeroed = pmm_alloc(0, PMM_FLAG_ZERO);

    uintptr_t paddr = PAGE_PADDR(PAGE_FROM_BLOCK(zeroed));

    uint8_t *data = (uint8_t *) HHDM(paddr);
    for(size_t i = 0; i < PAGE_GRANULARITY; i++) ASSERT(data[i] == 0);
}

void __module_uninitialize() {
    log(LOG_LEVEL_INFO, "TEST_PMM", "Passed all VM tests");
}
