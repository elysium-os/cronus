#include "arch/page.h"
#include "arch/ptm.h"
#include "common/assert.h"
#include "common/log.h"
#include "lib/container.h"
#include "lib/math.h"
#include "memory/vm.h"

#define MAP(AS, OFFSET, CNT, PROT, CACHE) vm_map_anon((AS), (void *) (ARCH_PAGE_GRANULARITY * (OFFSET)), (ARCH_PAGE_GRANULARITY * (CNT)), (PROT), (CACHE), VM_FLAG_FIXED)
#define UNMAP(AS, OFFSET, CNT) vm_unmap((AS), (void *) (ARCH_PAGE_GRANULARITY * (OFFSET)), (ARCH_PAGE_GRANULARITY * (CNT)))

typedef struct {
    uintptr_t offset;
    size_t count;
} as_check_t;

static bool check_as(vm_address_space_t *as, int n, ...) {
    va_list list;
    va_start(list, n);

    uintptr_t addr = 0;
    for(int i = 0; i < n; i++) {
        as_check_t chk = va_arg(list, as_check_t);

        rb_node_t *node = rb_search(&as->regions, addr, RB_SEARCH_TYPE_NEAREST_GTE);
        if(node == nullptr) {
            va_end(list);
            return false;
        }


        vm_region_t *region = CONTAINER_OF(node, vm_region_t, rb_node);
        if(region->base != chk.offset * ARCH_PAGE_GRANULARITY || region->length != chk.count * ARCH_PAGE_GRANULARITY) {
            va_end(list);
            return false;
        }

        addr = region->base + region->length;
    }

    rb_node_t *node = rb_search(&as->regions, addr, RB_SEARCH_TYPE_NEAREST_GTE);
    va_end(list);
    return node == nullptr;
}

void __module_initialize() {
    log(LOG_LEVEL_INFO, "TEST_MODULE", "Running VM tests");

    vm_address_space_t *as = arch_ptm_address_space_create();

    ASSERT(as->regions.root == nullptr);

    // Add one region
    ASSERT(MAP(as, 5, 1, VM_PROT_RW, VM_CACHE_STANDARD) != nullptr);
    ASSERT(check_as(as, 1, (as_check_t) { .offset = 5, .count = 1 }));

    // Test coalescing
    ASSERT(MAP(as, 6, 1, VM_PROT_RW, VM_CACHE_STANDARD) != nullptr);
    ASSERT(MAP(as, 9, 1, VM_PROT_RW, VM_CACHE_STANDARD) != nullptr);
    ASSERT(MAP(as, 7, 1, VM_PROT_RW, VM_CACHE_STANDARD) != nullptr);
    ASSERT(MAP(as, 8, 1, VM_PROT_RW, VM_CACHE_STANDARD) != nullptr);
    ASSERT(MAP(as, 10, 5, VM_PROT_RW, VM_CACHE_STANDARD) != nullptr);

    ASSERT(check_as(as, 1, (as_check_t) { .offset = 5, .count = 10 }));

    // Try unmapping
    UNMAP(as, 8, 4);

    ASSERT(check_as(as, 2, (as_check_t) { .offset = 5, .count = 3 }, (as_check_t) { .offset = 12, .count = 3 }));

    // Fill the gap
    ASSERT(MAP(as, 8, 4, VM_PROT_NONE, VM_CACHE_STANDARD) != nullptr);

    ASSERT(check_as(as, 3, (as_check_t) { .offset = 5, .count = 3 }, (as_check_t) { .offset = 8, .count = 4 }, (as_check_t) { .offset = 12, .count = 3 }));

    // Test rewrite
    vm_rewrite_prot(as, (void *) (ARCH_PAGE_GRANULARITY * 7), ARCH_PAGE_GRANULARITY * 4, VM_PROT_RWX);

    ASSERT(check_as(as, 4, (as_check_t) { .offset = 5, .count = 2 }, (as_check_t) { .offset = 7, .count = 4 }, (as_check_t) { .offset = 11, .count = 1 }, (as_check_t) { .offset = 12, .count = 3 }));

    vm_rewrite_prot(as, (void *) (ARCH_PAGE_GRANULARITY * 3), ARCH_PAGE_GRANULARITY * 9, VM_PROT_RW);

    ASSERT(check_as(as, 1, (as_check_t) { .offset = 5, .count = 10 }));

    // Unmap everything
    vm_unmap(as, (void *) as->start, MATH_FLOOR(as->end - as->start, ARCH_PAGE_GRANULARITY));
    ASSERT(as->regions.root == nullptr);

    // TODO: free address space
}

void __module_uninitialize() {
    log(LOG_LEVEL_INFO, "TEST_VM", "Passed all VM tests");
}
