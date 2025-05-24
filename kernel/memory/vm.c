#include "vm.h"

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

vm_address_space_t *g_vm_global_address_space;

static spinlock_t g_region_cache_lock = SPINLOCK_INIT;
static list_t g_region_cache = LIST_INIT;

static_assert(ARCH_PAGE_GRANULARITY > (sizeof(vm_region_t) * 2));

/**
 * @brief Find last region within a segment.
 */
static vm_region_t *find_region(vm_address_space_t *address_space, uintptr_t address, size_t length) {
    rb_node_t *node = rb_search(&address_space->regions, address + length, RB_SEARCH_TYPE_NEAREST_LT);
    if(node == nullptr) return nullptr;

    vm_region_t *region = CONTAINER_OF(node, vm_region_t, rb_node);
    if(region->base + region->length > address) return region;

    return nullptr;
}

/**
 * @brief Find a hole (free space) in an address space.
 * @warning Assumes address space lock is acquired.
 * @returns true = hole found, false = not found
 */
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
    for(size_t i = 0; i < length; i += ARCH_PAGE_GRANULARITY) {
        uintptr_t virtual_address = address + i;
        uintptr_t physical_address = 0;
        switch(region->type) {
            case VM_REGION_TYPE_ANON:   physical_address = PAGE_PADDR(PAGE_FROM_BLOCK(pmm_alloc_page(region->type_data.anon.back_zeroed ? PMM_FLAG_ZERO : PMM_FLAG_NONE))); break;
            case VM_REGION_TYPE_DIRECT: physical_address = region->type_data.direct.physical_address + (virtual_address - region->base); break;
        }
        arch_ptm_map(region->address_space, virtual_address, physical_address, region->protection, region->cache_behavior, is_global ? VM_PRIVILEGE_KERNEL : VM_PRIVILEGE_USER, is_global);
    }
}

static void region_unmap(vm_region_t *region, uintptr_t address, uintptr_t length) {
    ASSERT(address % ARCH_PAGE_GRANULARITY == 0 && length % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(address < region->base || address + length >= region->base);

    switch(region->type) {
        case VM_REGION_TYPE_ANON:
            // OPTIMIZE: invent page cache for anon regions because the current way is extremely slow
            // TODO: unmap phys mem
            break;
        case VM_REGION_TYPE_DIRECT: break;
    }
    for(uintptr_t i = 0; i < length; i += ARCH_PAGE_GRANULARITY) arch_ptm_unmap(region->address_space, address + i);
}

static vm_region_t *region_alloc(bool global_lock_acquired) {
    interrupt_state_t previous_state = spinlock_acquire(&g_region_cache_lock);
    if(g_region_cache.count == 0) {
        pmm_block_t *page = pmm_alloc_page(PMM_FLAG_ZERO);
        if(!global_lock_acquired) spinlock_primitive_acquire(&g_vm_global_address_space->lock);

        uintptr_t address;
        bool result = find_hole(g_vm_global_address_space, 0, ARCH_PAGE_GRANULARITY, &address);
        ASSERT_COMMENT(result, "out of global address space");

        arch_ptm_map(g_vm_global_address_space, address, PAGE_PADDR(PAGE_FROM_BLOCK(page)), (vm_protection_t) { .read = true, .write = true }, VM_CACHE_STANDARD, VM_PRIVILEGE_KERNEL, true);

        vm_region_t *region = (vm_region_t *) address;
        region[0].address_space = g_vm_global_address_space;
        region[0].type = VM_REGION_TYPE_ANON;
        region[0].base = address;
        region[0].length = ARCH_PAGE_GRANULARITY;
        region[0].protection = (vm_protection_t) { .read = true, .write = true };
        region[0].cache_behavior = VM_CACHE_STANDARD;

        rb_insert(&g_vm_global_address_space->regions, &region[0].rb_node);
        if(!global_lock_acquired) spinlock_primitive_release(&g_vm_global_address_space->lock);

        for(unsigned int i = 1; i < ARCH_PAGE_GRANULARITY / sizeof(vm_region_t); i++) list_push(&g_region_cache, &region[i].list_node);
    }

    list_node_t *node = list_pop(&g_region_cache);
    spinlock_release(&g_region_cache_lock, previous_state);
    return CONTAINER_OF(node, vm_region_t, list_node);
}

static void region_free(vm_region_t *region) {
    interrupt_state_t previous_state = spinlock_acquire(&g_region_cache_lock);
    list_push(&g_region_cache, &region->list_node);
    spinlock_release(&g_region_cache_lock, previous_state);
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
    if(region == nullptr) return false;
    region_map(region, MATH_FLOOR(vaddr, ARCH_PAGE_GRANULARITY), ARCH_PAGE_GRANULARITY);
    return true;
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
    log(LOG_LEVEL_DEBUG, "VM", "map(hint: %#lx, length: %#lx, prot: %c%c%c, flags: %lu, cache: %u, type: %u)", (uintptr_t) hint, length, prot.read ? 'R' : '-', prot.write ? 'W' : '-', prot.exec ? 'E' : '-', flags, cache, type);

    uintptr_t address = (uintptr_t) hint;
    if(length == 0 || length % ARCH_PAGE_GRANULARITY != 0) return nullptr;
    if(address % ARCH_PAGE_GRANULARITY != 0) {
        if(flags & VM_FLAG_FIXED) return nullptr;
        address += ARCH_PAGE_GRANULARITY - (address % ARCH_PAGE_GRANULARITY);
    }

    vm_region_t *region = region_alloc(false);
    interrupt_state_t previous_state = spinlock_acquire(&address_space->lock);
    bool result = find_hole(address_space, address, length, &address);
    if(!result || ((uintptr_t) hint != address && (flags & VM_FLAG_FIXED) != 0)) {
        region_free(region);
        spinlock_release(&address_space->lock, previous_state);
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

    switch(region->type) {
        case VM_REGION_TYPE_ANON: region->type_data.anon.back_zeroed = (flags & VM_FLAG_ZERO) != 0; break;
        case VM_REGION_TYPE_DIRECT:
            ASSERT(direct_physical_address % ARCH_PAGE_GRANULARITY == 0);
            region->type_data.direct.physical_address = direct_physical_address;
            break;
    }

    if((flags & VM_FLAG_NO_DEMAND) != 0) region_map(region, region->base, region->length);

    rb_insert(&address_space->regions, &region->rb_node);
    spinlock_release(&address_space->lock, previous_state);

    log(LOG_LEVEL_DEBUG, "VM", "map success (base: %#lx, length: %#lx)", region->base, region->length);
    return (void *) region->base;
}

void *vm_map_anon(vm_address_space_t *address_space, void *hint, size_t length, vm_protection_t prot, vm_cache_t cache, vm_flags_t flags) {
    return map_common(address_space, hint, length, prot, cache, flags, VM_REGION_TYPE_ANON, 0);
}

void *vm_map_direct(vm_address_space_t *address_space, void *hint, size_t length, vm_protection_t prot, vm_cache_t cache, uintptr_t physical_address, vm_flags_t flags) {
    return map_common(address_space, hint, length, prot, cache, flags, VM_REGION_TYPE_DIRECT, physical_address);
}

void vm_unmap(vm_address_space_t *address_space, void *address, size_t length) {
    log(LOG_LEVEL_DEBUG, "VM", "unmap(address: %#lx, length: %#lx)", (uintptr_t) address, length);
    if(length == 0) return;

    ASSERT((uintptr_t) address % ARCH_PAGE_GRANULARITY == 0 && length % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(SEGMENT_IN_BOUNDS((uintptr_t) address, length, address_space->start, address_space->end));

    interrupt_state_t previous_state = spinlock_acquire(&address_space->lock);
    for(uintptr_t split_base = (uintptr_t) address, split_length = 0; split_base < (uintptr_t) address + length; split_base += split_length) {
        split_length = ARCH_PAGE_GRANULARITY;
        vm_region_t *split_region = addr_to_region(address_space, split_base);
        if(split_region == nullptr) continue;

        while(ADDRESS_IN_SEGMENT(split_base + split_length, split_region->base, split_region->length) && ADDRESS_IN_SEGMENT(split_base + split_length, (uintptr_t) address, length)) split_length += ARCH_PAGE_GRANULARITY;

        ASSERT(SEGMENT_IN_BOUNDS(split_base, split_length, address_space->start, address_space->end));
        ASSERT(split_base % ARCH_PAGE_GRANULARITY == 0 && split_length % ARCH_PAGE_GRANULARITY == 0);

        region_unmap(split_region, split_base, split_length);

        if(split_region->base + split_region->length > split_base + split_length) {
            vm_region_t *region = region_alloc(address_space == g_vm_global_address_space);
            region->address_space = address_space;
            region->base = split_base + split_length;
            region->length = (split_region->base + split_region->length) - (split_base + split_length);
            region->protection = split_region->protection;
            region->type = split_region->type;
            region->type_data = split_region->type_data;

            rb_insert(&address_space->regions, &region->rb_node);
        }

        if(split_region->base < split_base) {
            split_region->length = split_base - split_region->base;
        } else {
            rb_remove(&address_space->regions, &split_region->rb_node);
            region_free(split_region);
        }
    }
    spinlock_release(&address_space->lock, previous_state);
}

bool vm_fault(uintptr_t address, vm_fault_t fault) {
    if(fault != VM_FAULT_NOT_PRESENT) return false;
    if(ADDRESS_IN_BOUNDS(address, g_vm_global_address_space->start, g_vm_global_address_space->end)) return false;

    process_t *proc = arch_sched_thread_current()->proc;
    if(proc == nullptr) return false;

    return address_space_fix_page(proc->address_space, address);
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

        size_t len = math_min(count - i, ARCH_PAGE_GRANULARITY - offset);
        memcpy((void *) HHDM(phys + offset), src, len);
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

        size_t len = math_min(count - i, ARCH_PAGE_GRANULARITY - offset);
        memcpy(dest, (void *) HHDM(phys + offset), len);
        i += len;
        dest += len;
    }
    return i;
}
