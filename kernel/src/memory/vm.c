#include "vm.h"

#include "arch/page.h"
#include "arch/ptm.h"
#include "common/assert.h"
#include "common/log.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "memory/hhdm.h"
#include "memory/pmm.h"

#define ADDRESS_IN_BOUNDS(ADDRESS, START, END) ((ADDRESS) >= (START) && (ADDRESS) < (END))
#define SEGMENT_IN_BOUNDS(BASE, LENGTH, START, END) (ADDRESS_IN_BOUNDS((BASE), (START), (END)) && ((END) - (BASE)) >= (LENGTH))
#define ADDRESS_IN_SEGMENT(ADDRESS, BASE, LENGTH) ((ADDRESS) >= (BASE) && (ADDRESS) < ((BASE) + (LENGTH)))
#define SEGMENT_INTERSECTS(BASE1, LENGTH1, BASE2, LENGTH2) ((BASE1) < ((BASE2) + (LENGTH2)) && (BASE2) < ((BASE1) + (LENGTH1)))

vm_address_space_t *g_vm_global_address_space;

static spinlock_t g_free_regions_lock = SPINLOCK_INIT;
static list_t g_free_regions = LIST_INIT;

static_assert(ARCH_PAGE_GRANULARITY > (sizeof(vm_region_t) * 2));

/** @warning Assumes address space lock is acquired. */
static uintptr_t find_space(vm_address_space_t *address_space, uintptr_t address, size_t length) {
    if(!SEGMENT_IN_BOUNDS(address, length, address_space->start, address_space->end)) address = address_space->start;
    while(true) {
        if(!SEGMENT_IN_BOUNDS(address, length, address_space->start, address_space->end)) return 0;
        bool valid = true;
        LIST_FOREACH(&address_space->regions, elem) {
            vm_region_t *region = LIST_CONTAINER_GET(elem, vm_region_t, list_elem);
            if(!SEGMENT_INTERSECTS(address, length, region->base, region->length)) continue;
            valid = false;
            address = region->base + region->length;
            break;
        }
        if(valid) break;
    }
    return address;
}

static void region_map(vm_region_t *region, uintptr_t address, uintptr_t length) {
    ASSERT(address % ARCH_PAGE_GRANULARITY == 0 && length % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(address < region->base || address + length >= region->base);

    bool is_global = region->address_space == g_vm_global_address_space;
    vm_privilege_t privilege = is_global ? VM_PRIVILEGE_KERNEL : VM_PRIVILEGE_USER;

    for(size_t i = 0; i < length; i += ARCH_PAGE_GRANULARITY) {
        uintptr_t virtual_address = address + i;
        uintptr_t physical_address = 0;
        switch(region->type) {
            case VM_REGION_TYPE_ANON:
                pmm_flags_t physical_flags = PMM_FLAG_NONE;
                if(region->type_data.anon.back_zeroed) physical_flags |= PMM_FLAG_ZERO;
                physical_address = pmm_alloc_page(PMM_ZONE_NORMAL, physical_flags)->paddr;
                break;
            case VM_REGION_TYPE_DIRECT: physical_address = region->type_data.direct.physical_address + (virtual_address - region->base); break;
        }
        arch_ptm_map(region->address_space, virtual_address, physical_address, region->protection, region->cache_behavior, privilege, is_global);
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
    ipl_t regions_previous_ipl = spinlock_acquire(&g_free_regions_lock);
    if(list_is_empty(&g_free_regions)) {
        ipl_t as_previous_ipl;
        pmm_page_t *page = pmm_alloc_page(PMM_ZONE_NORMAL, PMM_FLAG_ZERO);
        if(!global_lock_acquired) as_previous_ipl = spinlock_acquire(&g_vm_global_address_space->lock);
        uintptr_t address = find_space(g_vm_global_address_space, 0, ARCH_PAGE_GRANULARITY);
        arch_ptm_map(g_vm_global_address_space, address, page->paddr, (vm_protection_t) {.read = true, .write = true}, VM_CACHE_STANDARD, VM_PRIVILEGE_KERNEL, true);

        vm_region_t *region = (vm_region_t *) address;
        region[0].address_space = g_vm_global_address_space;
        region[0].type = VM_REGION_TYPE_ANON;
        region[0].base = address;
        region[0].length = ARCH_PAGE_GRANULARITY;
        region[0].protection = (vm_protection_t) {.read = true, .write = true};
        region[0].cache_behavior = VM_CACHE_STANDARD;

        list_append(&g_vm_global_address_space->regions, &region[0].list_elem);
        if(!global_lock_acquired) spinlock_release(&g_vm_global_address_space->lock, as_previous_ipl);

        for(unsigned int i = 1; i < ARCH_PAGE_GRANULARITY / sizeof(vm_region_t); i++) list_append(&g_free_regions, &region[i].list_elem);
    }
    list_element_t *elem = LIST_NEXT(&g_free_regions);
    ASSERT(elem != NULL);
    list_delete(elem);
    spinlock_release(&g_free_regions_lock, regions_previous_ipl);
    return LIST_CONTAINER_GET(elem, vm_region_t, list_elem);
}

static void region_free(vm_region_t *region) {
    ipl_t previous_ipl = spinlock_acquire(&g_free_regions_lock);
    list_append(&g_free_regions, &region->list_elem);
    spinlock_release(&g_free_regions_lock, previous_ipl);
}

static vm_region_t *addr_to_region(vm_address_space_t *address_space, uintptr_t address) {
    if(!ADDRESS_IN_BOUNDS(address, address_space->start, address_space->end)) return NULL;
    LIST_FOREACH(&address_space->regions, elem) {
        vm_region_t *region = LIST_CONTAINER_GET(elem, vm_region_t, list_elem);
        if(!ADDRESS_IN_SEGMENT(address, region->base, region->length)) continue;
        return region;
    }
    return NULL;
}

static bool address_space_fix_page(vm_address_space_t *address_space, uintptr_t vaddr) {
    vm_region_t *region = addr_to_region(address_space, vaddr);
    if(region == NULL) return false;
    region_map(region, MATH_FLOOR(vaddr, ARCH_PAGE_GRANULARITY), ARCH_PAGE_GRANULARITY);
    return true;
}

// OPTIMIZE: this is very slow/inefficient, segments should probably be ordered or in a tree and we could do this fast
static bool memory_exists(vm_address_space_t *address_space, uintptr_t address, size_t length) {
    if(!ADDRESS_IN_BOUNDS(address, address_space->start, address_space->end) || !ADDRESS_IN_BOUNDS(address + length, address_space->start, address_space->end)) return false;
restart:
    if(length == 0) return true;
    LIST_FOREACH(&address_space->regions, elem) {
        vm_region_t *region = LIST_CONTAINER_GET(elem, vm_region_t, list_elem);
        if(region->base <= address && region->base + region->length > address) {
            uintptr_t new_addr = region->base + region->length;
            if(new_addr >= address + length) return true;
            length = (address + length) - new_addr;
            address = new_addr;
            goto restart;
        }
        if(region->base > address && region->base < address + length && region->base + region->length >= address + length) {
            length -= (address + length) - region->base;
            goto restart;
        }
    }
    return false;
}

static void *map_common(
    vm_address_space_t *address_space,
    void *hint,
    size_t length,
    vm_protection_t prot,
    vm_cache_t cache,
    vm_flags_t flags,
    vm_region_type_t type,
    uintptr_t direct_physical_address
) {
    log(LOG_LEVEL_DEBUG,
        "VM",
        "map(hint: %#lx, length: %#lx, prot: %c%c%c, flags: %lu, cache: %u, type: %u)",
        (uintptr_t) hint,
        length,
        prot.read ? 'R' : '-',
        prot.write ? 'W' : '-',
        prot.exec ? 'E' : '-',
        flags,
        cache,
        type);

    uintptr_t address = (uintptr_t) hint;
    if(length == 0 || length % ARCH_PAGE_GRANULARITY != 0) return NULL;
    if(address % ARCH_PAGE_GRANULARITY != 0) {
        if(flags & VM_FLAG_FIXED) return NULL;
        address += ARCH_PAGE_GRANULARITY - (address % ARCH_PAGE_GRANULARITY);
    }

    vm_region_t *region = region_alloc(false);
    ipl_t as_previous_ipl = spinlock_acquire(&address_space->lock);
    address = find_space(address_space, address, length);
    if(address == 0 || ((uintptr_t) hint != address && (flags & VM_FLAG_FIXED) != 0)) {
        region_free(region);
        spinlock_release(&address_space->lock, as_previous_ipl);
        return NULL;
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
        case VM_REGION_TYPE_ANON:   region->type_data.anon.back_zeroed = (flags & VM_FLAG_ZERO) != 0; break;
        case VM_REGION_TYPE_DIRECT: region->type_data.direct.physical_address = direct_physical_address; break;
    }

    if((flags & VM_FLAG_NO_DEMAND) != 0) region_map(region, region->base, region->length);

    list_append(&address_space->regions, &region->list_elem);
    spinlock_release(&address_space->lock, as_previous_ipl);

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
    log(LOG_LEVEL_DEBUG, "VMM", "unmap(address: %#lx, length: %#lx)", (uintptr_t) address, length);
    if(length == 0) return;

    ASSERT((uintptr_t) address % ARCH_PAGE_GRANULARITY == 0 && length % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(SEGMENT_IN_BOUNDS((uintptr_t) address, length, address_space->start, address_space->end));

    ipl_t as_previous_ipl = spinlock_acquire(&address_space->lock);
    for(uintptr_t split_base = (uintptr_t) address, split_length = 0; split_base < (uintptr_t) address + length; split_base += split_length) {
        split_length = ARCH_PAGE_GRANULARITY;
        vm_region_t *split_region = addr_to_region(address_space, split_base);
        if(split_region == NULL) continue;

        while(ADDRESS_IN_SEGMENT(split_base + split_length, split_region->base, split_region->length) &&
              ADDRESS_IN_SEGMENT(split_base + split_length, (uintptr_t) address, length))
            split_length += ARCH_PAGE_GRANULARITY;

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
            switch(region->type) {
                case VM_REGION_TYPE_ANON:   break;
                case VM_REGION_TYPE_DIRECT: region->type_data = split_region->type_data; break;
            }

            list_append(&split_region->list_elem, &region->list_elem);
        }

        if(split_region->base < split_base) {
            split_region->length = split_base - split_region->base;
        } else {
            list_delete(&split_region->list_elem);
            region_free(split_region);
        }
    }
    spinlock_release(&address_space->lock, as_previous_ipl);
}

bool vm_fault(uintptr_t address, vm_fault_t fault) {
    if(fault != VM_FAULT_NOT_PRESENT) return false;

    vm_address_space_t *as = g_vm_global_address_space;
    if(!ADDRESS_IN_BOUNDS(address, g_vm_global_address_space->start, g_vm_global_address_space->end)) {
        // TODO
        // if(x86_64_init_stage() >= X86_64_INIT_STAGE_SCHED) {
        //     process_t *proc = arch_sched_thread_current()->proc;
        //     if(proc) as = proc->address_space;
        // }
    }

    return address_space_fix_page(as, address);
}

size_t vm_copy_to(vm_address_space_t *dest_as, uintptr_t dest_addr, void *src, size_t count) {
    if(!memory_exists(dest_as, dest_addr, count)) return 0;
    size_t i = 0;
    while(i < count) {
        size_t offset = (dest_addr + i) % ARCH_PAGE_GRANULARITY;
        uintptr_t phys;
        if(!arch_ptm_physical(dest_as, dest_addr + i, &phys)) {
            if(!address_space_fix_page(dest_as, dest_addr + i)) return i;
            ASSERT(arch_ptm_physical(dest_as, dest_addr + i, &phys));
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
            ASSERT(arch_ptm_physical(src_as, src_addr + i, &phys));
        }

        size_t len = math_min(count - i, ARCH_PAGE_GRANULARITY - offset);
        memcpy(dest, (void *) HHDM(phys + offset), len);
        i += len;
        dest += len;
    }
    return i;
}
