#include "memory/mmio.h"

#include "arch/page.h"
#include "lib/math.h"
#include "memory/vm.h"

void *mmio_map(uintptr_t address, uintptr_t length) {
    size_t offset = address % ARCH_PAGE_GRANULARITY;
    if(offset != 0) {
        address -= offset;
        length += offset;
    }

    return (void *) ((uintptr_t) vm_map_direct(g_vm_global_address_space, nullptr, MATH_CEIL(length, ARCH_PAGE_GRANULARITY), VM_PROT_RW, VM_CACHE_NONE, address, VM_FLAG_NONE) + offset);
}

void mmio_unmap(void *address, uintptr_t length) {
    size_t offset = (uintptr_t) address % ARCH_PAGE_GRANULARITY;
    if(offset != 0) {
        address -= offset;
        length += offset;
    }

    vm_unmap(g_vm_global_address_space, address, MATH_CEIL(length, ARCH_PAGE_GRANULARITY));
}
