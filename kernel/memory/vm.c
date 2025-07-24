#include "vm.h"

#include "arch/mem.h"
#include "arch/page.h"
#include "arch/ptm.h"
#include "arch/sched.h"
#include "common/assert.h"
#include "common/log.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "lib/param.h"
#include "memory/hhdm.h"
#include "memory/page.h"
#include "memory/pmm.h"
#include "sched/process.h"

#define ADDRESS_IN_BOUNDS(ADDRESS, START, END) ((ADDRESS) >= (START) && (ADDRESS) < (END))
#define SEGMENT_IN_BOUNDS(BASE, LENGTH, START, END) (ADDRESS_IN_BOUNDS((BASE), (START), (END)) && ((END) - (BASE)) >= (LENGTH))
#define ADDRESS_IN_SEGMENT(ADDRESS, BASE, LENGTH) ((ADDRESS) >= (BASE) && (ADDRESS) < ((BASE) + (LENGTH)))
#define SEGMENT_INTERSECTS(BASE1, LENGTH1, BASE2, LENGTH2) ((BASE1) < ((BASE2) + (LENGTH2)) && (BASE2) < ((BASE1) + (LENGTH1)))

#define PROT_EQUALS(P1, P2) ((P1)->read == (P2)->read && (P1)->write == (P2)->write && (P1)->exec == (P2)->exec)

typedef enum {
    REWRITE_TYPE_DELETE,
    REWRITE_TYPE_PROTECTION,
    REWRITE_TYPE_CACHE
} rewrite_type_t;

vm_address_space_t *g_vm_global_address_space;

static spinlock_t g_region_cache_lock = SPINLOCK_INIT;
static list_t g_region_cache = LIST_INIT;

static_assert(ARCH_PAGE_GRANULARITY > (sizeof(vm_region_t) * 2));

static vm_region_t *region_insert(vm_address_space_t *address_space, vm_region_t *region);

static rb_value_t region_node_value(rb_node_t *node) {
    return CONTAINER_OF(node, vm_region_t, rb_node)->base;
}

/// Find last region within a segment.
static vm_region_t *find_region(vm_address_space_t *address_space, uintptr_t address, size_t length) {
    rb_node_t *node = rb_search(&address_space->regions, address + length, RB_SEARCH_TYPE_NEAREST_LT);
    if(node == nullptr) return nullptr;

    vm_region_t *region = CONTAINER_OF(node, vm_region_t, rb_node);
    if(region->base + region->length > address) return region;

    return nullptr;
}

/// Find a hole (free space) in an address space.
/// @warning Assumes address space lock is acquired.
/// @returns true = hole found, false = not found
static bool find_hole(vm_address_space_t *address_space, uintptr_t address, size_t length, PARAM_OUT(uintptr_t *) hole) {
    if(SEGMENT_IN_BOUNDS(address, length, address_space->start, address_space->end) && find_region(address_space, address, length) == nullptr) {
        *hole = address;
        return true;
    }

    address = address_space->start;
    while(true) {
        if(!SEGMENT_IN_BOUNDS(address, length, address_space->start, address_space->end)) return false;

        vm_region_t *region = find_region(address_space, address, length);
        if(region == nullptr) {
            *hole = address;
            return true;
        }

        address = region->base + region->length;
    }
    return false;
}

static void region_map(vm_region_t *region, uintptr_t address, uintptr_t length) {
    ASSERT(address % ARCH_PAGE_GRANULARITY == 0 && length % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(address < region->base || address + length >= region->base);

    bool is_global = region->address_space == g_vm_global_address_space;
    switch(region->type) {
        case VM_REGION_TYPE_ANON:
            for(size_t i = 0; i < length; i += ARCH_PAGE_GRANULARITY) {
                uintptr_t virtual_address = address + i;
                uintptr_t physical_address = PAGE_PADDR(PAGE_FROM_BLOCK(pmm_alloc_page(region->type_data.anon.back_zeroed ? PMM_FLAG_ZERO : PMM_FLAG_NONE)));
                arch_ptm_map(region->address_space, virtual_address, physical_address, ARCH_PAGE_GRANULARITY, region->protection, region->cache_behavior, is_global ? VM_PRIVILEGE_KERNEL : VM_PRIVILEGE_USER, is_global);
            }
            break;
        case VM_REGION_TYPE_DIRECT:
            arch_ptm_map(region->address_space, address, region->type_data.direct.physical_address + (address - region->base), length, region->protection, region->cache_behavior, is_global ? VM_PRIVILEGE_KERNEL : VM_PRIVILEGE_USER, is_global);
            break;
    }
}

static void region_unmap(vm_region_t *region, uintptr_t address, uintptr_t length) {
    ASSERT(address % ARCH_PAGE_GRANULARITY == 0 && length % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(address >= region->base && address + length <= region->base + region->length);

    switch(region->type) {
        case VM_REGION_TYPE_ANON:
            // OPTIMIZE: invent page cache for anon regions because the current way is extremely slow
            // TODO: unmap phys mem
            break;
        case VM_REGION_TYPE_DIRECT: break;
    }
    arch_ptm_unmap(region->address_space, address, length);
}

/// Check whether the flags of a region are compatible with each other.
static bool regions_mergeable(vm_region_t *left, vm_region_t *right) {
    if(left->type != right->type) return false;
    if(left->base + left->length != right->base) return false;
    if(!PROT_EQUALS(&left->protection, &right->protection)) return false;
    if(left->cache_behavior != right->cache_behavior) return false;
    if(left->dynamically_backed != right->dynamically_backed) return false;

    switch(left->type) {
        case VM_REGION_TYPE_ANON:
            if(left->type_data.anon.back_zeroed != right->type_data.anon.back_zeroed) return false;
            break;
        case VM_REGION_TYPE_DIRECT:
            if(!SEGMENT_IN_BOUNDS(left->type_data.direct.physical_address, left->length, 0, ARCH_MEM_PHYS_MAX)) return false;
            if(left->type_data.direct.physical_address + left->length != right->type_data.direct.physical_address) return false;
            break;
    }

    return true;
}

/// Allocate a region from the internal vm region pool.
static vm_region_t *region_alloc(bool global_lock_acquired) {
    spinlock_acquire_nodw(&g_region_cache_lock);
    if(g_region_cache.count == 0) {
        spinlock_release_nodw(&g_region_cache_lock);

        pmm_block_t *page = pmm_alloc_page(PMM_FLAG_ZERO);
        if(!global_lock_acquired) spinlock_acquire_nodw(&g_vm_global_address_space->lock);

        uintptr_t address;
        if(!find_hole(g_vm_global_address_space, 0, ARCH_PAGE_GRANULARITY, &address)) panic("VM", "out of global address space");

        arch_ptm_map(g_vm_global_address_space, address, PAGE_PADDR(PAGE_FROM_BLOCK(page)), ARCH_PAGE_GRANULARITY, VM_PROT_RW, VM_CACHE_STANDARD, VM_PRIVILEGE_KERNEL, true);

        vm_region_t *region = (vm_region_t *) address;
        region[0].address_space = g_vm_global_address_space;
        region[0].type = VM_REGION_TYPE_ANON;
        region[0].base = address;
        region[0].length = ARCH_PAGE_GRANULARITY;
        region[0].protection = VM_PROT_RW;
        region[0].cache_behavior = VM_CACHE_STANDARD;
        region[0].dynamically_backed = false;

        region_insert(g_vm_global_address_space, &region[0]);
        if(!global_lock_acquired) spinlock_release_nodw(&g_vm_global_address_space->lock);

        spinlock_acquire_nodw(&g_region_cache_lock);
        for(unsigned int i = 1; i < ARCH_PAGE_GRANULARITY / sizeof(vm_region_t); i++) list_push(&g_region_cache, &region[i].list_node);
    }

    list_node_t *node = list_pop(&g_region_cache);
    spinlock_release_nodw(&g_region_cache_lock);
    return CONTAINER_OF(node, vm_region_t, list_node);
}

/// Free region into the internal vm region pool.
static void region_free(vm_region_t *region) {
    spinlock_acquire_nodw(&g_region_cache_lock);
    list_push(&g_region_cache, &region->list_node);
    spinlock_release_nodw(&g_region_cache_lock);
}

/// Create a region by cloning another to a new region.
static vm_region_t *clone_to(bool global_lock_acquired, uintptr_t base, size_t length, vm_region_t *from) {
    vm_region_t *region = region_alloc(global_lock_acquired);
    region->type = from->type;
    region->address_space = from->address_space;
    region->cache_behavior = from->cache_behavior;
    region->protection = from->protection;
    region->dynamically_backed = from->dynamically_backed;

    switch(from->type) {
        case VM_REGION_TYPE_ANON: region->type_data.anon.back_zeroed = from->type_data.anon.back_zeroed; break;
        case VM_REGION_TYPE_DIRECT:
            uintptr_t new_physical_address = from->type_data.direct.physical_address;
            if(base > from->base) {
                ASSERT(UINTPTR_MAX - new_physical_address > base - from->base);
                new_physical_address += base - from->base;
            } else {
                ASSERT(new_physical_address > from->base - base);
                new_physical_address -= from->base - base;
            }
            region->type_data.direct.physical_address = new_physical_address;
            break;
    }

    region->base = base;
    region->length = length;
    return region;
}

static vm_region_t *region_insert(vm_address_space_t *address_space, vm_region_t *region) {
    rb_node_t *right_node = rb_search(&address_space->regions, region->base, RB_SEARCH_TYPE_NEAREST_GT);
    if(right_node != nullptr) {
        vm_region_t *right = CONTAINER_OF(right_node, vm_region_t, rb_node);
        if(regions_mergeable(region, right)) {
            region->length += right->length;
            rb_remove(&address_space->regions, &right->rb_node);
            region_free(right);
        }
    }
    rb_insert(&address_space->regions, &region->rb_node);

    rb_node_t *left_node = rb_search(&address_space->regions, region->base, RB_SEARCH_TYPE_NEAREST_LT);
    if(left_node != nullptr) {
        vm_region_t *left = CONTAINER_OF(left_node, vm_region_t, rb_node);
        if(regions_mergeable(left, region)) {
            left->length += region->length;
            rb_remove(&address_space->regions, &region->rb_node);
            region_free(region);
            return left;
        }
    }

    return region;
}

static vm_region_t *addr_to_region(vm_address_space_t *address_space, uintptr_t address) {
    if(!ADDRESS_IN_BOUNDS(address, address_space->start, address_space->end)) return nullptr;

    rb_node_t *node = rb_search(&address_space->regions, address, RB_SEARCH_TYPE_NEAREST_LTE);
    if(node == nullptr) return nullptr;

    vm_region_t *region = CONTAINER_OF(node, vm_region_t, rb_node);
    if(!ADDRESS_IN_SEGMENT(address, region->base, region->length)) return nullptr;

    return region;
}

static bool address_space_fix_page(vm_address_space_t *address_space, uintptr_t vaddr) {
    vm_region_t *region = addr_to_region(address_space, vaddr);
    if(region == nullptr || !region->dynamically_backed) return false;
    region_map(region, MATH_FLOOR(vaddr, ARCH_PAGE_GRANULARITY), ARCH_PAGE_GRANULARITY);
    return true;
}

static void vm_fault_soft(void *data) {
    thread_t *thread = data;

    ASSERT(thread->proc != nullptr);

    bool ok = address_space_fix_page(thread->proc->address_space, thread->vm_fault.address);
    if(!ok) panic("VM", "vm_fault_soft handling failed for (pid: %lu, tid: %lu) on %#lx", thread->proc->id, thread->id, thread->vm_fault.address);

    thread->vm_fault.in_flight = false;
}

static bool memory_exists(vm_address_space_t *address_space, uintptr_t address, size_t length) {
    if(!ADDRESS_IN_BOUNDS(address, address_space->start, address_space->end) || !ADDRESS_IN_BOUNDS(address + length, address_space->start, address_space->end)) return false;

    uintptr_t top = address + length;
    while(top > address) {
        // OPTIMIZE: (low priority) this can technically be improved by manually walking
        // the btree instead of doing many binary searches.
        rb_node_t *node = rb_search(&address_space->regions, top, RB_SEARCH_TYPE_NEAREST_LT);
        if(node == nullptr) return false;

        vm_region_t *region = CONTAINER_OF(node, vm_region_t, rb_node);
        if(region->base + region->length < top) return false;

        top = region->base;
    }
    return true;
}

static void *map_common(vm_address_space_t *address_space, void *hint, size_t length, vm_protection_t prot, vm_cache_t cache, vm_flags_t flags, vm_region_type_t type, uintptr_t direct_physical_address) {
    LOG_TRACE("VM", "map(hint: %#lx, length: %#lx, prot: %c%c%c, flags: %lu, cache: %u, type: %u)", (uintptr_t) hint, length, prot.read ? 'R' : '-', prot.write ? 'W' : '-', prot.exec ? 'E' : '-', flags, cache, type);

    uintptr_t address = (uintptr_t) hint;
    if(length == 0 || length % ARCH_PAGE_GRANULARITY != 0) return nullptr;
    if(address % ARCH_PAGE_GRANULARITY != 0) {
        if(flags & VM_FLAG_FIXED) return nullptr;
        address += ARCH_PAGE_GRANULARITY - (address % ARCH_PAGE_GRANULARITY);
    }

    vm_region_t *region = region_alloc(false);
    spinlock_acquire_nodw(&address_space->lock);
    bool result = find_hole(address_space, address, length, &address);
    if(!result || ((uintptr_t) hint != address && (flags & VM_FLAG_FIXED) != 0)) {
        region_free(region);
        spinlock_release_nodw(&address_space->lock);
        return nullptr;
    }

    ASSERT(SEGMENT_IN_BOUNDS(address, length, address_space->start, address_space->end));
    ASSERT(address % ARCH_PAGE_GRANULARITY == 0 && length % ARCH_PAGE_GRANULARITY == 0);

    region->address_space = address_space;
    region->type = type;
    region->base = address;
    region->length = length;
    region->protection = prot;
    region->cache_behavior = cache;
    region->dynamically_backed = (flags & VM_FLAG_DYNAMICALLY_BACKED) != 0;

    switch(region->type) {
        case VM_REGION_TYPE_ANON: region->type_data.anon.back_zeroed = (flags & VM_FLAG_ZERO) != 0; break;
        case VM_REGION_TYPE_DIRECT:
            ASSERT(direct_physical_address % ARCH_PAGE_GRANULARITY == 0);
            region->type_data.direct.physical_address = direct_physical_address;
            LOG_TRACE("VM", "physical address of region: %#lx", direct_physical_address);
            break;
    }

    if(!region->dynamically_backed) region_map(region, region->base, region->length);

    region = region_insert(address_space, region);

    spinlock_release_nodw(&address_space->lock);

    LOG_TRACE("VM", "map success (base: %#lx, length: %#lx)", address, length);
    return (void *) address;
}

static void rewrite_common(vm_address_space_t *address_space, void *address, size_t length, rewrite_type_t type, vm_protection_t prot, vm_cache_t cache) {
    LOG_TRACE("VM", "rewrite(as_start: %#lx, address: %#lx, length: %#lx)", address_space->start, (uintptr_t) address, length);
    if(length == 0) return;

    ASSERT((uintptr_t) address % ARCH_PAGE_GRANULARITY == 0 && length % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(SEGMENT_IN_BOUNDS((uintptr_t) address, length, address_space->start, address_space->end));

    spinlock_acquire_nodw(&address_space->lock);

    uintptr_t current_address = (uintptr_t) address;

    rb_node_t *node = rb_search(&address_space->regions, current_address, RB_SEARCH_TYPE_NEAREST_LTE);
    if(node != nullptr) {
        vm_region_t *split_region = CONTAINER_OF(node, vm_region_t, rb_node);
        if(split_region->base + split_region->length > current_address) {
            uintptr_t split_base = current_address;
            size_t split_length = (split_region->base + split_region->length) - split_base;
            if(split_length > length) split_length = length;

            switch(type) {
                case REWRITE_TYPE_DELETE: region_unmap(split_region, split_base, split_length); goto l_no_clone;
                case REWRITE_TYPE_CACHE:
                    if(split_region->cache_behavior == cache) goto l_skip;
                    break;
                case REWRITE_TYPE_PROTECTION:
                    if(PROT_EQUALS(&split_region->protection, &prot)) goto l_skip;
                    break;
            }

            vm_region_t *region = clone_to(address_space == g_vm_global_address_space, split_base, split_length, split_region);

        l_no_clone:

            if(split_region->base + split_region->length > split_base + split_length) {
                uintptr_t new_base = split_base + split_length;
                size_t new_length = (split_region->base + split_region->length) - (split_base + split_length);
                rb_insert(&address_space->regions, &clone_to(address_space == g_vm_global_address_space, new_base, new_length, split_region)->rb_node);
            }

            if(split_base > split_region->base) {
                split_region->length = split_base - split_region->base;
            } else {
                rb_remove(&address_space->regions, &split_region->rb_node);
                region_free(split_region);
            }

            switch(type) {
                case REWRITE_TYPE_DELETE:     goto l_skip;
                case REWRITE_TYPE_CACHE:      region->cache_behavior = cache; break;
                case REWRITE_TYPE_PROTECTION: region->protection = prot; break;
            }

            region = region_insert(address_space, region);

            bool is_global = region->address_space == g_vm_global_address_space;
            arch_ptm_rewrite(region->address_space, split_base, split_length, region->protection, region->cache_behavior, is_global ? VM_PRIVILEGE_KERNEL : VM_PRIVILEGE_USER, is_global);

        l_skip:

            current_address = split_base + split_length;
        }
    }

    while(current_address < ((uintptr_t) address + length)) {
        node = rb_search(&address_space->regions, current_address, RB_SEARCH_TYPE_NEAREST_GTE);
        if(node == nullptr) break;

        vm_region_t *split_region = CONTAINER_OF(node, vm_region_t, rb_node);
        current_address = split_region->base + split_region->length;

        size_t split_length = ((uintptr_t) address + length) - split_region->base;
        if(split_length > split_region->length) split_length = split_region->length;

        switch(type) {
            case REWRITE_TYPE_DELETE: region_unmap(split_region, split_region->base, split_length); goto r_no_clone;
            case REWRITE_TYPE_CACHE:
                if(split_region->cache_behavior == cache) goto r_skip;
                break;
            case REWRITE_TYPE_PROTECTION:
                if(PROT_EQUALS(&split_region->protection, &prot)) goto r_skip;
                break;
        }

        uintptr_t split_base = split_region->base;
        vm_region_t *region = clone_to(address_space == g_vm_global_address_space, split_base, split_length, split_region);

    r_no_clone:

        if(split_region->length > split_length) {
            uintptr_t new_base = split_base + split_length;
            size_t new_length = split_region->length - split_length;
            rb_insert(&address_space->regions, &clone_to(address_space == g_vm_global_address_space, new_base, new_length, split_region)->rb_node);
        }
        rb_remove(&address_space->regions, &split_region->rb_node);
        region_free(split_region);

        switch(type) {
            case REWRITE_TYPE_DELETE:     goto r_skip;
            case REWRITE_TYPE_CACHE:      region->cache_behavior = cache; break;
            case REWRITE_TYPE_PROTECTION: region->protection = prot; break;
        }

        region = region_insert(address_space, region);

        bool is_global = region->address_space == g_vm_global_address_space;
        arch_ptm_rewrite(region->address_space, split_base, split_length, region->protection, region->cache_behavior, is_global ? VM_PRIVILEGE_KERNEL : VM_PRIVILEGE_USER, is_global);

    r_skip:
    }
    spinlock_release_nodw(&address_space->lock);
}

void *vm_map_anon(vm_address_space_t *address_space, void *hint, size_t length, vm_protection_t prot, vm_cache_t cache, vm_flags_t flags) {
    return map_common(address_space, hint, length, prot, cache, flags, VM_REGION_TYPE_ANON, 0);
}

void *vm_map_direct(vm_address_space_t *address_space, void *hint, size_t length, vm_protection_t prot, vm_cache_t cache, uintptr_t physical_address, vm_flags_t flags) {
    return map_common(address_space, hint, length, prot, cache, flags, VM_REGION_TYPE_DIRECT, physical_address);
}

void vm_unmap(vm_address_space_t *address_space, void *address, size_t length) {
    rewrite_common(address_space, address, length, REWRITE_TYPE_DELETE, (vm_protection_t) {}, VM_CACHE_STANDARD);
}

void vm_rewrite_prot(vm_address_space_t *address_space, void *address, size_t length, vm_protection_t prot) {
    rewrite_common(address_space, address, length, REWRITE_TYPE_PROTECTION, prot, VM_CACHE_STANDARD);
}

void vm_rewrite_cache(vm_address_space_t *address_space, void *address, size_t length, vm_cache_t cache) {
    rewrite_common(address_space, address, length, REWRITE_TYPE_CACHE, (vm_protection_t) {}, cache);
}

bool vm_fault(uintptr_t address, vm_fault_t fault) {
    if(fault != VM_FAULT_NOT_PRESENT) return false;
    if(ADDRESS_IN_BOUNDS(address, g_vm_global_address_space->start, g_vm_global_address_space->end)) return false;

    process_t *proc = arch_sched_thread_current()->proc;
    if(proc == nullptr) return false;

    thread_t *current_thread = arch_sched_thread_current();
    ASSERT(!current_thread->vm_fault.in_flight);

    current_thread->vm_fault.in_flight = true;
    current_thread->vm_fault.address = address;
    current_thread->vm_fault.dw_item.data = current_thread;
    current_thread->vm_fault.dw_item.fn = vm_fault_soft;

    dw_queue(&current_thread->vm_fault.dw_item);

    return true;
}

size_t vm_copy_to(vm_address_space_t *dest_as, uintptr_t dest_addr, void *src, size_t count) {
    if(!memory_exists(dest_as, dest_addr, count)) return 0;
    size_t i = 0;
    while(i < count) {
        size_t offset = (dest_addr + i) % ARCH_PAGE_GRANULARITY;
        uintptr_t phys;
        if(!arch_ptm_physical(dest_as, dest_addr + i, &phys)) {
            if(!address_space_fix_page(dest_as, dest_addr + i)) return i;
            bool success = arch_ptm_physical(dest_as, dest_addr + i, &phys);
            ASSERT(success);
        }

        size_t len = MATH_MIN(count - i, ARCH_PAGE_GRANULARITY - offset);
        memcpy((void *) HHDM(phys), src, len);
        i += len;
        src += len;
    }
    return i;
}

size_t vm_copy_from(void *dest, vm_address_space_t *src_as, uintptr_t src_addr, size_t count) {
    if(!memory_exists(src_as, src_addr, count)) return 0;
    size_t i = 0;
    while(i < count) {
        size_t offset = (src_addr + i) % ARCH_PAGE_GRANULARITY;
        uintptr_t phys;
        if(!arch_ptm_physical(src_as, src_addr + i, &phys)) {
            if(!address_space_fix_page(src_as, src_addr + i)) return i;
            bool success = arch_ptm_physical(src_as, src_addr + i, &phys);
            ASSERT(success);
        }

        size_t len = MATH_MIN(count - i, ARCH_PAGE_GRANULARITY - offset);
        memcpy(dest, (void *) HHDM(phys), len);
        i += len;
        dest += len;
    }
    return i;
}

rb_tree_t vm_create_regions() {
    return RB_TREE_INIT(region_node_value);
}
