#include "ptm.h"

#include "arch/page.h"
#include "arch/ptm.h"
#include "common/assert.h"
#include "common/log.h"
#include "common/spinlock.h"
#include "lib/container.h"
#include "lib/mem.h"
#include "memory/hhdm.h"
#include "memory/pmm.h"

#include "arch/x86_64/exception.h"
#include "arch/x86_64/init.h"
#include "arch/x86_64/sys/cpu.h"
#include "arch/x86_64/sys/cr.h"

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

static uint64_t privilege_to_x86_flags(vm_privilege_t privilege) {
    switch(privilege) {
        case VM_PRIVILEGE_KERNEL: return 0;
        case VM_PRIVILEGE_USER:   return PTE_FLAG_USER;
    }
    __builtin_unreachable();
}

static uint64_t cache_to_x86_flags(vm_cache_t cache) {
    switch(cache) {
        case VM_CACHE_STANDARD:      return PTE_PAT0;
        case VM_CACHE_WRITE_COMBINE: return PTE_PAT4;
    }
    __builtin_unreachable();
}

vm_address_space_t *x86_64_ptm_init() {
    g_initial_address_space.common.lock = SPINLOCK_INIT;
    g_initial_address_space.common.regions = LIST_INIT;
    g_initial_address_space.common.start = KERNELSPACE_START;
    g_initial_address_space.common.end = KERNELSPACE_END;
    g_initial_address_space.cr3 = pmm_alloc_page(PMM_ZONE_NORMAL, PMM_FLAG_ZERO)->paddr;
    g_initial_address_space.cr3_lock = SPINLOCK_INIT;

    uint64_t *old_pml4 = (uint64_t *) HHDM(x86_64_cr3_read());
    uint64_t *pml4 = (uint64_t *) HHDM(g_initial_address_space.cr3);
    for(int i = 256; i < 512; i++) {
        if(old_pml4[i] & PTE_FLAG_PRESENT) {
            pml4[i] = old_pml4[i];
            continue;
        }

        // Needs to be completely unrestricted as these are not synchronized across address spaces
        pml4[i] = PTE_FLAG_PRESENT | PTE_FLAG_RW | (pmm_alloc_page(PMM_ZONE_NORMAL, PMM_FLAG_ZERO)->paddr & ADDRESS_MASK);
    }

    return &g_initial_address_space.common;
}

void arch_ptm_load_address_space(vm_address_space_t *address_space) {
    x86_64_cr3_write(X86_64_AS(address_space)->cr3);
}

void arch_ptm_map(vm_address_space_t *address_space, uintptr_t vaddr, uintptr_t paddr, vm_protection_t prot, vm_cache_t cache, vm_privilege_t privilege, bool global) {
    if(!prot.read) log(LOG_LEVEL_ERROR, "PTM", "No-read mapping is not supported on x86_64");
    spinlock_acquire(&X86_64_AS(address_space)->cr3_lock);
    uint64_t *current_table = (uint64_t *) HHDM(X86_64_AS(address_space)->cr3);
    for(int i = 4; i > 1; i--) {
        int index = VADDR_TO_INDEX(vaddr, i);
        if((current_table[index] & PTE_FLAG_PRESENT) == 0) {
            pmm_page_t *page = pmm_alloc_page(PMM_ZONE_NORMAL, PMM_FLAG_ZERO);
            current_table[index] = PTE_FLAG_PRESENT | (page->paddr & ADDRESS_MASK);
            if(!prot.exec) current_table[index] |= PTE_FLAG_NX;
        } else {
            if(prot.exec) current_table[index] &= ~PTE_FLAG_NX;
        }
        if(prot.write) current_table[index] |= PTE_FLAG_RW;
        current_table[index] |= privilege_to_x86_flags(privilege);
        current_table = (uint64_t *) HHDM(current_table[index] & ADDRESS_MASK);
    }
    int index = VADDR_TO_INDEX(vaddr, 1);
    current_table[index] = PTE_FLAG_PRESENT | (paddr & ADDRESS_MASK) | privilege_to_x86_flags(privilege) | cache_to_x86_flags(cache);
    if(prot.write) current_table[index] |= PTE_FLAG_RW;
    if(!prot.exec) current_table[index] |= PTE_FLAG_NX;
    if(global) current_table[index] |= PTE_FLAG_GLOBAL;

    // tlb_shootdown(address_space, vaddr); // TODO

    spinlock_release(&X86_64_AS(address_space)->cr3_lock);
}

void arch_ptm_unmap(vm_address_space_t *address_space, uintptr_t vaddr) {
    spinlock_acquire(&X86_64_AS(address_space)->cr3_lock);
    uint64_t *current_table = (uint64_t *) HHDM(X86_64_AS(address_space)->cr3);
    for(int i = 4; i > 1; i--) {
        int index = VADDR_TO_INDEX(vaddr, i);
        if((current_table[index] & PTE_FLAG_PRESENT) != 0) {
            spinlock_release(&X86_64_AS(address_space)->cr3_lock);
            return;
        }
        current_table = (uint64_t *) HHDM(current_table[index] & ADDRESS_MASK);
    }
    current_table[VADDR_TO_INDEX(vaddr, 1)] = 0;

    // tlb_shootdown(address_space, vaddr); // TODO

    spinlock_release(&X86_64_AS(address_space)->cr3_lock);
}

bool arch_ptm_physical(vm_address_space_t *address_space, uintptr_t vaddr, uintptr_t *out) {
    spinlock_acquire(&X86_64_AS(address_space)->cr3_lock);
    uint64_t *current_table = (uint64_t *) HHDM(X86_64_AS(address_space)->cr3);
    for(int i = 4; i > 1; i--) {
        int index = VADDR_TO_INDEX(vaddr, i);
        if((current_table[index] & PTE_FLAG_PRESENT) != 0) {
            spinlock_release(&X86_64_AS(address_space)->cr3_lock);
            return false;
        }
        current_table = (uint64_t *) HHDM(current_table[index] & ADDRESS_MASK);
    }
    uint64_t entry = current_table[VADDR_TO_INDEX(vaddr, 1)];
    spinlock_release(&X86_64_AS(address_space)->cr3_lock);
    if((entry & PTE_FLAG_PRESENT) != 0) return false;
    *out = (entry & ADDRESS_MASK);
    return true;
}

void x86_64_ptm_page_fault_handler(x86_64_interrupt_frame_t *frame) {
    vm_fault_t fault;
    if((frame->err_code & PAGEFAULT_FLAG_PRESENT) == 0) fault = VM_FAULT_NOT_PRESENT;

    if(vm_fault(x86_64_cr2_read(), fault)) return;
    x86_64_exception_unhandled(frame);
}