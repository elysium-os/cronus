#include "arch/page.h"
#include "arch/ptm.h"
#include "common/assert.h"
#include "common/log.h"
#include "lib/container.h"
#include "lib/macros.h"
#include "lib/math.h"
#include "lib/rb.h"
#include "memory/vm.h"

#define MAP(AS, OFFSET, CNT, PROT, CACHE) vm_map_anon((AS), (void *) (ARCH_PAGE_GRANULARITY * (OFFSET)), (ARCH_PAGE_GRANULARITY * (CNT)), (PROT), (CACHE), VM_FLAG_FIXED)
#define UNMAP(AS, OFFSET, CNT) vm_unmap((AS), (void *) (ARCH_PAGE_GRANULARITY * (OFFSET)), (ARCH_PAGE_GRANULARITY * (CNT)))
#define TEST_ASSERT(AS, ASSERT)                                                                    \
    if(!(ASSERT)) {                                                                                \
        print_as((AS), LOG_LEVEL_FATAL);                                                           \
        panic("TEST_VM", "VM test failed `%s` " __FILE__ ":" MACROS_STRINGIFY(__LINE__), #ASSERT); \
    }

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

static void print_as(vm_address_space_t *as, log_level_t log_level) {
    log(log_level, "TEST_VM", "Address space:");
    uintptr_t cur = 0;
rep:
    rb_node_t *node = rb_search(&as->regions, cur, RB_SEARCH_TYPE_NEAREST_GTE);
    if(node == nullptr) return;

    vm_region_t *region = CONTAINER_OF(node, vm_region_t, rb_node);
    log(log_level, "TEST_VM", " - VmRegion { base: %#lx, length: %#lx, type: %u }", region->base, region->length, region->type);

    cur = region->base + region->length;
    goto rep;
}

void __module_initialize() {
    log(LOG_LEVEL_INFO, "TEST_VM", "Running VM tests");

    vm_address_space_t *as = arch_ptm_address_space_create();

    TEST_ASSERT(as, as->regions.root == nullptr);

    // Setup region for unmap tests + coalesce tests
    TEST_ASSERT(as, MAP(as, 1, 5, VM_PROT_RW, VM_CACHE_STANDARD) != nullptr);
    TEST_ASSERT(as, check_as(as, 1, (as_check_t) { .offset = 1, .count = 5 }));

    // Unmap middle
    UNMAP(as, 3, 1);
    TEST_ASSERT(as, check_as(as, 2, (as_check_t) { .offset = 1, .count = 2 }, (as_check_t) { .offset = 4, .count = 2 }));

    TEST_ASSERT(as, MAP(as, 3, 1, VM_PROT_RW, VM_CACHE_STANDARD) != nullptr);
    TEST_ASSERT(as, check_as(as, 1, (as_check_t) { .offset = 1, .count = 5 }));

    // Unmap left
    print_as(as, LOG_LEVEL_INFO);
    UNMAP(as, 1, 1);
    print_as(as, LOG_LEVEL_INFO);
    TEST_ASSERT(as, check_as(as, 1, (as_check_t) { .offset = 2, .count = 4 }));

    TEST_ASSERT(as, MAP(as, 1, 1, VM_PROT_RW, VM_CACHE_STANDARD) != nullptr);
    TEST_ASSERT(as, check_as(as, 1, (as_check_t) { .offset = 1, .count = 5 }));

    // Unmap right
    UNMAP(as, 5, 1);
    TEST_ASSERT(as, check_as(as, 1, (as_check_t) { .offset = 1, .count = 4 }));

    TEST_ASSERT(as, MAP(as, 5, 1, VM_PROT_RW, VM_CACHE_STANDARD) != nullptr);
    TEST_ASSERT(as, check_as(as, 1, (as_check_t) { .offset = 1, .count = 5 }));

    // Unmap entire
    UNMAP(as, 1, 5);
    TEST_ASSERT(as, check_as(as, 0));
    TEST_ASSERT(as, as->regions.root == nullptr);


    // Add one region
    TEST_ASSERT(as, MAP(as, 5, 1, VM_PROT_RW, VM_CACHE_STANDARD) != nullptr);
    TEST_ASSERT(as, check_as(as, 1, (as_check_t) { .offset = 5, .count = 1 }));

    // Test coalescing
    TEST_ASSERT(as, MAP(as, 6, 1, VM_PROT_RW, VM_CACHE_STANDARD) != nullptr);
    TEST_ASSERT(as, MAP(as, 9, 1, VM_PROT_RW, VM_CACHE_STANDARD) != nullptr);
    TEST_ASSERT(as, MAP(as, 7, 1, VM_PROT_RW, VM_CACHE_STANDARD) != nullptr);
    TEST_ASSERT(as, MAP(as, 8, 1, VM_PROT_RW, VM_CACHE_STANDARD) != nullptr);
    TEST_ASSERT(as, MAP(as, 10, 5, VM_PROT_RW, VM_CACHE_STANDARD) != nullptr);

    TEST_ASSERT(as, check_as(as, 1, (as_check_t) { .offset = 5, .count = 10 }));

    // Try unmapping
    UNMAP(as, 8, 4);

    TEST_ASSERT(as, check_as(as, 2, (as_check_t) { .offset = 5, .count = 3 }, (as_check_t) { .offset = 12, .count = 3 }));

    // Fill the gap
    TEST_ASSERT(as, MAP(as, 8, 4, VM_PROT_RX, VM_CACHE_STANDARD) != nullptr);

    TEST_ASSERT(as, check_as(as, 3, (as_check_t) { .offset = 5, .count = 3 }, (as_check_t) { .offset = 8, .count = 4 }, (as_check_t) { .offset = 12, .count = 3 }));

    // Test rewrite
    vm_rewrite_prot(as, (void *) (ARCH_PAGE_GRANULARITY * 7), ARCH_PAGE_GRANULARITY * 4, VM_PROT_RWX);

    TEST_ASSERT(as, check_as(as, 4, (as_check_t) { .offset = 5, .count = 2 }, (as_check_t) { .offset = 7, .count = 4 }, (as_check_t) { .offset = 11, .count = 1 }, (as_check_t) { .offset = 12, .count = 3 }));

    vm_rewrite_prot(as, (void *) (ARCH_PAGE_GRANULARITY * 3), ARCH_PAGE_GRANULARITY * 9, VM_PROT_RW);

    TEST_ASSERT(as, check_as(as, 1, (as_check_t) { .offset = 5, .count = 10 }));

    // Unmap everything
    vm_unmap(as, (void *) as->start, MATH_FLOOR(as->end - as->start, ARCH_PAGE_GRANULARITY));
    TEST_ASSERT(as, as->regions.root == nullptr);

    // TODO: free address space
}

void __module_uninitialize() {
    log(LOG_LEVEL_INFO, "TEST_VM", "Passed all VM tests");
}
