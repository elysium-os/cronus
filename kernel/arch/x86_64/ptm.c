#include "ptm.h"

#include "arch/page.h"
#include "arch/ptm.h"
#include "common/assert.h"
#include "common/lock/spinlock.h"
#include "common/log.h"
#include "lib/container.h"
#include "lib/mem.h"
#include "memory/heap.h"
#include "memory/hhdm.h"

#include "arch/x86_64/cpu/cr.h"
#include "arch/x86_64/exception.h"
#include "arch/x86_64/init.h"
#include "arch/x86_64/tlb.h"

#include <stddef.h>
#include <stdint.h>

#define X86_64_AS(ADDRESS_SPACE) (CONTAINER_OF((ADDRESS_SPACE), x86_64_address_space_t, common))

#define VADDR_TO_INDEX(VADDR, LEVEL) (((VADDR) >> ((LEVEL) * 9 + 3)) & 0x1FF)
#define ADDRESS_MASK ((uint64_t) 0x000F'FFFF'FFFF'F000)

#define PTE_FLAG_PRESENT (1 << 0)
#define PTE_FLAG_RW (1 << 1)
#define PTE_FLAG_USER (1 << 2)
#define PTE_FLAG_WRITETHROUGH (1 << 3)
#define PTE_FLAG_DISABLECACHE (1 << 4)
#define PTE_FLAG_ACCESSED (1 << 5)
#define PTE_FLAG_PAT (1 << 7)
#define PTE_FLAG_GLOBAL (1 << 8)
#define PTE_FLAG_NX ((uint64_t) 1 << 63)

#define PTE_PAT0 0
#define PTE_PAT1 PTE_FLAG_WRITETHROUGH
#define PTE_PAT2 PTE_FLAG_DISABLECACHE
#define PTE_PAT3 PTE_FLAG_DISABLECACHE | PTE_FLAG_WRITETHROUGH
#define PTE_PAT4 PTE_FLAG_PAT
#define PTE_PAT5 PTE_FLAG_PAT | PTE_FLAG_WRITETHROUGH
#define PTE_PAT6 PTE_FLAG_PAT | PTE_FLAG_DISABLECACHE
#define PTE_PAT7 PTE_FLAG_PAT | PTE_FLAG_DISABLECACHE | PTE_FLAG_WRITETHROUGH

#define PAGEFAULT_FLAG_PRESENT (1 << 0)
#define PAGEFAULT_FLAG_WRITE (1 << 1)
#define PAGEFAULT_FLAG_USER (1 << 2)
#define PAGEFAULT_FLAG_RESERVED_WRITE (1 << 3)
#define PAGEFAULT_FLAG_INSTRUCTION_FETCH (1 << 4)
#define PAGEFAULT_FLAG_PROTECTION_KEY (1 << 5)
#define PAGEFAULT_FLAG_SHADOW_STACK (1 << 6)
#define PAGEFAULT_FLAG_SGX (1 << 7)

#define KERNELSPACE_START 0xFFFF'8000'0000'0000
#define KERNELSPACE_END (UINT64_MAX - ARCH_PAGE_GRANULARITY)
#define USERSPACE_START (ARCH_PAGE_GRANULARITY)
#define USERSPACE_END (((uintptr_t) 1 << 47) - ARCH_PAGE_GRANULARITY - 1)

typedef struct {
    spinlock_t cr3_lock;
    uintptr_t cr3;
    vm_address_space_t common;
} x86_64_address_space_t;

static x86_64_address_space_t g_initial_address_space;

uintptr_t (*g_x86_64_ptm_phys_allocator)();

static uint64_t privilege_to_x86_flags(vm_privilege_t privilege) {
    switch(privilege) {
        case VM_PRIVILEGE_KERNEL: return 0;
        case VM_PRIVILEGE_USER:   return PTE_FLAG_USER;
    }
    ASSERT_UNREACHABLE();
}

static uint64_t cache_to_x86_flags(vm_cache_t cache) {
    switch(cache) {
        case VM_CACHE_STANDARD:      return PTE_PAT0;
        case VM_CACHE_WRITE_COMBINE: return PTE_PAT6;
        case VM_CACHE_NONE:          return PTE_PAT3;
    }
    ASSERT_UNREACHABLE();
}

static rb_value_t region_node_value(rb_node_t *node) {
    return CONTAINER_OF(node, vm_region_t, rb_node)->base;
}

vm_address_space_t *x86_64_ptm_init() {
    g_initial_address_space.common.lock = SPINLOCK_INIT;
    g_initial_address_space.common.regions = RB_TREE_INIT(region_node_value);
    g_initial_address_space.common.start = KERNELSPACE_START;
    g_initial_address_space.common.end = KERNELSPACE_END;
    g_initial_address_space.cr3 = g_x86_64_ptm_phys_allocator();
    g_initial_address_space.cr3_lock = SPINLOCK_INIT;

    uint64_t *old_pml4 = (uint64_t *) HHDM(x86_64_cr3_read());
    uint64_t *pml4 = (uint64_t *) HHDM(g_initial_address_space.cr3);
    for(int i = 256; i < 512; i++) {
        if(old_pml4[i] & PTE_FLAG_PRESENT) {
            pml4[i] = old_pml4[i];
            continue;
        }

        // Needs to be completely unrestricted as these are not synchronized across address spaces
        pml4[i] = PTE_FLAG_PRESENT | PTE_FLAG_RW | (g_x86_64_ptm_phys_allocator() & ADDRESS_MASK);
    }

    return &g_initial_address_space.common;
}

vm_address_space_t *arch_ptm_address_space_create() {
    x86_64_address_space_t *address_space = heap_alloc(sizeof(x86_64_address_space_t));
    address_space->cr3 = g_x86_64_ptm_phys_allocator();
    address_space->cr3_lock = SPINLOCK_INIT;
    address_space->common.lock = SPINLOCK_INIT;
    address_space->common.regions = RB_TREE_INIT(region_node_value);
    address_space->common.start = USERSPACE_START;
    address_space->common.end = USERSPACE_END;

    memcpy((void *) HHDM(address_space->cr3 + 256 * sizeof(uint64_t)), (void *) HHDM(X86_64_AS(g_vm_global_address_space)->cr3 + 256 * sizeof(uint64_t)), 256 * sizeof(uint64_t));

    return &address_space->common;
}

void arch_ptm_load_address_space(vm_address_space_t *address_space) {
    x86_64_cr3_write(X86_64_AS(address_space)->cr3);
}

void arch_ptm_map(vm_address_space_t *address_space, uintptr_t vaddr, uintptr_t paddr, size_t length, vm_protection_t prot, vm_cache_t cache, vm_privilege_t privilege, bool global) {
    ASSERT(vaddr % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(paddr % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(length % ARCH_PAGE_GRANULARITY == 0);

    if(!prot.read) log(LOG_LEVEL_ERROR, "PTM", "No-read mapping is not supported on x86_64");
    interrupt_state_t previous_state = spinlock_acquire(&X86_64_AS(address_space)->cr3_lock);

    for(size_t i = 0; i < length; i += ARCH_PAGE_GRANULARITY) {
        uint64_t *current_table = (uint64_t *) HHDM(X86_64_AS(address_space)->cr3);
        for(int j = 4; j > 1; j--) {
            int index = VADDR_TO_INDEX(vaddr + i, j);
            uint64_t entry = current_table[index];
            if((entry & PTE_FLAG_PRESENT) == 0) {
                entry = PTE_FLAG_PRESENT | (g_x86_64_ptm_phys_allocator() & ADDRESS_MASK);
                if(!prot.exec) entry |= PTE_FLAG_NX;
            } else {
                if(prot.exec) entry &= ~PTE_FLAG_NX;
            }
            if(prot.write) entry |= PTE_FLAG_RW;
            entry |= privilege_to_x86_flags(privilege);
            __atomic_store(&current_table[index], &entry, __ATOMIC_SEQ_CST);

            current_table = (uint64_t *) HHDM(current_table[index] & ADDRESS_MASK);
        }
        uint64_t entry = PTE_FLAG_PRESENT | ((paddr + i) & ADDRESS_MASK) | privilege_to_x86_flags(privilege) | cache_to_x86_flags(cache);
        if(prot.write) entry |= PTE_FLAG_RW;
        if(!prot.exec) entry |= PTE_FLAG_NX;
        if(global) entry |= PTE_FLAG_GLOBAL;
        __atomic_store(&current_table[VADDR_TO_INDEX(vaddr + i, 1)], &entry, __ATOMIC_SEQ_CST);
    }

    x86_64_tlb_shootdown(vaddr, length);

    spinlock_release(&X86_64_AS(address_space)->cr3_lock, previous_state);
}

void arch_ptm_rewrite(vm_address_space_t *address_space, uintptr_t vaddr, size_t length, vm_protection_t prot, vm_cache_t cache, vm_privilege_t privilege, bool global) {
    ASSERT(vaddr % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(length % ARCH_PAGE_GRANULARITY == 0);

    if(!prot.read) log(LOG_LEVEL_ERROR, "PTM", "No-read mapping is not supported on x86_64");
    interrupt_state_t previous_state = spinlock_acquire(&X86_64_AS(address_space)->cr3_lock);

    for(size_t i = 0; i < length; i += ARCH_PAGE_GRANULARITY) {
        uint64_t *current_table = (uint64_t *) HHDM(X86_64_AS(address_space)->cr3);
        for(int j = 4; j > 1; j--) {
            int index = VADDR_TO_INDEX(vaddr + i, j);
            uint64_t entry = current_table[index];
            if((entry & PTE_FLAG_PRESENT) == 0) goto skip;
            if(prot.write) entry |= PTE_FLAG_RW;
            if(prot.exec) entry &= ~PTE_FLAG_NX;
            current_table = (uint64_t *) HHDM(current_table[index] & ADDRESS_MASK);
        }
        int index = VADDR_TO_INDEX(vaddr + i, 1);
        uint64_t entry = current_table[index] | privilege_to_x86_flags(privilege) | cache_to_x86_flags(cache);

        if(prot.write)
            entry |= PTE_FLAG_RW;
        else
            entry &= ~PTE_FLAG_RW;

        if(!prot.exec)
            entry |= PTE_FLAG_NX;
        else
            entry &= ~PTE_FLAG_NX;

        if(global)
            entry |= PTE_FLAG_GLOBAL;
        else
            entry &= ~PTE_FLAG_GLOBAL;

        __atomic_store_n(&current_table[index], entry, __ATOMIC_SEQ_CST);
    skip:
    }

    x86_64_tlb_shootdown(vaddr, length);

    spinlock_release(&X86_64_AS(address_space)->cr3_lock, previous_state);
}

void arch_ptm_unmap(vm_address_space_t *address_space, uintptr_t vaddr, size_t length) {
    ASSERT(vaddr % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(length % ARCH_PAGE_GRANULARITY == 0);

    interrupt_state_t previous_state = spinlock_acquire(&X86_64_AS(address_space)->cr3_lock);

    for(size_t i = 0; i < length; i += ARCH_PAGE_GRANULARITY) {
        uint64_t *current_table = (uint64_t *) HHDM(X86_64_AS(address_space)->cr3);
        for(int j = 4; j > 1; j--) {
            int index = VADDR_TO_INDEX(vaddr + i, j);
            if((current_table[index] & PTE_FLAG_PRESENT) == 0) goto skip;
            current_table = (uint64_t *) HHDM(current_table[index] & ADDRESS_MASK);
        }
        __atomic_store_n(&current_table[VADDR_TO_INDEX(vaddr + i, 1)], 0, __ATOMIC_SEQ_CST);
    skip:
    }

    x86_64_tlb_shootdown(vaddr, length);

    spinlock_release(&X86_64_AS(address_space)->cr3_lock, previous_state);
}

bool arch_ptm_physical(vm_address_space_t *address_space, uintptr_t vaddr, PARAM_OUT(uintptr_t *) paddr) {
    interrupt_state_t previous_state = spinlock_acquire(&X86_64_AS(address_space)->cr3_lock);
    uint64_t *current_table = (uint64_t *) HHDM(X86_64_AS(address_space)->cr3);
    for(int i = 4; i > 1; i--) {
        int index = VADDR_TO_INDEX(vaddr, i);
        if((current_table[index] & PTE_FLAG_PRESENT) == 0) {
            spinlock_release(&X86_64_AS(address_space)->cr3_lock, previous_state);
            return false;
        }
        current_table = (uint64_t *) HHDM(current_table[index] & ADDRESS_MASK);
    }
    uint64_t entry = current_table[VADDR_TO_INDEX(vaddr, 1)];
    spinlock_release(&X86_64_AS(address_space)->cr3_lock, previous_state);
    if((entry & PTE_FLAG_PRESENT) == 0) return false;
    *paddr = (entry & ADDRESS_MASK);
    return true;
}

void x86_64_ptm_page_fault_handler(x86_64_interrupt_frame_t *frame) {
    vm_fault_t fault = VM_FAULT_UNKNOWN;
    if((frame->err_code & PAGEFAULT_FLAG_PRESENT) == 0) fault = VM_FAULT_NOT_PRESENT;

    if(x86_64_init_flag_check(X86_64_INIT_FLAG_SCHED) && vm_fault(x86_64_cr2_read(), fault)) return;
    x86_64_exception_unhandled(frame);
}
