#include "arch/page.h"
#include "common/log.h"
#include "memory/hhdm.h"
#include "memory/page.h"
#include "memory/pmm.h"

#define TEST_ASSERT(COND)                                                           \
    if(!(COND)) {                                                                   \
        log(LOG_LEVEL_ERROR, "TEST_MODULE", "Test assertion \"%s\" failed", #COND); \
        return false;                                                               \
    }

typedef struct {
    const char *name;
    bool (*fn)();
} test_t;

static bool test_pmm() {
    pmm_block_t *block = pmm_alloc(0, PMM_FLAG_NONE);
    TEST_ASSERT(block->order == 0);
    TEST_ASSERT(block->free == false);

    pmm_free(block);
    TEST_ASSERT(block->free == true);

    pmm_block_t *zeroed = pmm_alloc(0, PMM_FLAG_ZERO);

    uintptr_t paddr = PAGE_PADDR(PAGE_FROM_BLOCK(zeroed));

    uint8_t *data = (uint8_t *) HHDM(paddr);
    for(size_t i = 0; i < ARCH_PAGE_GRANULARITY; i++) TEST_ASSERT(data[i] == 0);

    return true;
}

void __module_initialize() {
    log(LOG_LEVEL_INFO, "TEST_MODULE", "Running System Tests");

    test_t tests[] = {
        { .name = "PMM", .fn = test_pmm }
    };

    for(size_t i = 0; i < sizeof(tests) / sizeof(test_t); i++) {
        log(LOG_LEVEL_INFO, "TEST_MODULE", "Running %s Tests", tests[i].name);

        if(!tests[i].fn()) log(LOG_LEVEL_ERROR, "TEST_MODULE", "Test Failed");
    }
}

void __module_uninitialize() {
    log(LOG_LEVEL_INFO, "TEST_MODULE", "Tearing Down Tests");
}
