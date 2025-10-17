#include "x86_64/ptm.h"

#include "arch/cpu.h"
#include "arch/page.h"
#include "arch/ptm.h"
#include "common/assert.h"
#include "common/lock/spinlock.h"
#include "common/log.h"
#include "lib/expect.h"
#include "lib/mem.h"
#include "memory/earlymem.h"
#include "memory/heap.h"
#include "memory/hhdm.h"
#include "memory/page.h"
#include "memory/pmm.h"
#include "sys/init.h"
#include "x86_64/cpu/cr.h"
#include "x86_64/exception.h"
#include "x86_64/interrupt.h"
#include "x86_64/tlb.h"

#include <stddef.h>
#include <stdint.h>

#define VADDR_TO_INDEX(VADDR, LEVEL) (((VADDR) >> ((LEVEL) * 9 + 3)) & 0x1FF)
#define INDEX_TO_VADDR(INDEX, LEVEL) ((uint64_t) (INDEX) << ((LEVEL) * 9 + 3))
#define LEVEL_TO_PAGESIZE(LEVEL) (1UL << (12 + 9 * ((LEVEL) - 1)))

#define LEVEL_COUNT 4

#define ENTRY_FLAG_PRESENT (1 << 0)
#define ENTRY_FLAG_RW (1 << 1)
#define ENTRY_FLAG_USER (1 << 2)
#define ENTRY_FLAG_WRITETHROUGH (1 << 3)
#define ENTRY_FLAG_DISABLECACHE (1 << 4)
#define ENTRY_FLAG_ACCESSED (1 << 5)
#define ENTRY_FLAG_GLOBAL (1 << 8)
#define ENTRY_FLAG_NX ((uint64_t) 1 << 63)
#define ENTRY_FLAG_PAT(PAGE_SIZE) ((PAGE_SIZE) == PAGE_SIZE_4K ? ENTRYL_FLAG_PAT : ENTRYH_FLAG_PAT)
#define ENTRY_ADDRESS_MASK(PAGE_SIZE) ((PAGE_SIZE) == PAGE_SIZE_4K ? ENTRYL_ADDRESS_MASK : ENTRYH_ADDRESS_MASK)

#define ENTRYL_FLAG_PAT (1 << 7)
#define ENTRYL_ADDRESS_MASK ((uint64_t) 0x000F'FFFF'FFFF'F000)

#define ENTRYH_FLAG_PS (1 << 7)
#define ENTRYH_FLAG_PAT (1 << 12)
#define ENTRYH_ADDRESS_MASK ((uint64_t) 0x000F'FFFF'FFFF'0000)

#define PAT0 (0)
#define PAT1 (ENTRY_FLAG_WRITETHROUGH)
#define PAT2 (ENTRY_FLAG_DISABLECACHE)
#define PAT3 (ENTRY_FLAG_DISABLECACHE | ENTRY_FLAG_WRITETHROUGH)
#define PAT4(PAT_FLAG) (PAT_FLAG)
#define PAT5(PAT_FLAG) ((PAT_FLAG) | ENTRY_FLAG_WRITETHROUGH)
#define PAT6(PAT_FLAG) ((PAT_FLAG) | ENTRY_FLAG_DISABLECACHE)
#define PAT7(PAT_FLAG) ((PAT_FLAG) | ENTRY_FLAG_DISABLECACHE | ENTRY_FLAG_WRITETHROUGH)

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

typedef enum {
    PAGE_SIZE_4K = ARCH_PAGE_SIZE_4KB,
    PAGE_SIZE_2M = ARCH_PAGE_SIZE_2MB,
    PAGE_SIZE_1G = ARCH_PAGE_SIZE_1GB,
} page_size_t;

static x86_64_ptm_address_space_t g_global_address_space;

static uintptr_t alloc_page() {
    if(EXPECT_UNLIKELY(g_earlymem_active)) {
        uintptr_t address = earlymem_alloc_page();
        memclear((void *) HHDM(address), ARCH_PAGE_GRANULARITY);
        return address;
    }
    return PAGE_PADDR(PAGE_FROM_BLOCK(pmm_alloc_page(PMM_FLAG_ZERO)));
}

static uint64_t privilege_to_x86_flags(vm_privilege_t privilege) {
    switch(privilege) {
        case VM_PRIVILEGE_KERNEL: return 0;
        case VM_PRIVILEGE_USER:   return ENTRY_FLAG_USER;
    }
    ASSERT_UNREACHABLE();
}

static uint64_t cache_to_x86_flags(vm_cache_t cache, page_size_t page_size) {
    switch(cache) {
        case VM_CACHE_STANDARD:      return PAT0;
        case VM_CACHE_WRITE_COMBINE: return PAT6(ENTRY_FLAG_PAT(page_size));
        case VM_CACHE_NONE:          return PAT3;
    }
    ASSERT_UNREACHABLE();
}

static uint64_t break_big(uint64_t *table, int index, int current_level) {
    ASSERT(current_level > 0);

    uint64_t entry = table[index];

    uintptr_t address = entry & ENTRYH_ADDRESS_MASK;
    bool pat = entry & ENTRYH_FLAG_PAT;
    entry &= ~ENTRYL_ADDRESS_MASK;

    uint64_t new_entry = entry;
    if(current_level - 1 == 0) {
        new_entry &= ~ENTRYL_FLAG_PAT;
        if(pat) new_entry |= ENTRYL_FLAG_PAT;
    } else {
        new_entry |= ENTRYH_FLAG_PAT;
    }

    entry |= alloc_page();
    if(pat) entry |= ENTRYL_FLAG_PAT;

    uint64_t *new_table = (void *) (entry & ENTRYL_ADDRESS_MASK);
    for(int i = 0; i < 512 * ARCH_PAGE_GRANULARITY; i += ARCH_PAGE_GRANULARITY) new_table[i] = new_entry | (address + i);

    __atomic_store(&table[index], &entry, __ATOMIC_SEQ_CST);

    return entry;
}

static void map_page(uint64_t *pml4, uintptr_t vaddr, uintptr_t paddr, page_size_t page_size, vm_protection_t prot, vm_cache_t cache, vm_privilege_t privilege, bool global) {
    int lowest_index;
    switch(page_size) {
        case PAGE_SIZE_4K: lowest_index = 1; break;
        case PAGE_SIZE_2M: lowest_index = 2; break;
        case PAGE_SIZE_1G: lowest_index = 3; break;
    }

    uint64_t *current_table = pml4;
    for(int j = LEVEL_COUNT; j > lowest_index; j--) {
        int index = VADDR_TO_INDEX(vaddr, j);

        uint64_t entry = current_table[index];
        if((entry & ENTRY_FLAG_PRESENT) == 0) {
            entry = ENTRY_FLAG_PRESENT | (alloc_page() & ENTRYL_ADDRESS_MASK);
            if(!prot.exec) entry |= ENTRY_FLAG_NX;
        } else {
            if((entry & ENTRYH_FLAG_PS) != 0) entry = break_big(current_table, index, j);
            if(prot.exec) entry &= ~ENTRY_FLAG_NX;
        }
        if(prot.write) entry |= ENTRY_FLAG_RW;
        entry |= privilege_to_x86_flags(privilege);
        __atomic_store(&current_table[index], &entry, __ATOMIC_SEQ_CST);

        current_table = (uint64_t *) HHDM(entry & ENTRYL_ADDRESS_MASK);
    }

    uint64_t entry = ENTRY_FLAG_PRESENT | (paddr & ENTRY_ADDRESS_MASK(page_size)) | privilege_to_x86_flags(privilege) | cache_to_x86_flags(cache, page_size);
    if(page_size != PAGE_SIZE_4K) entry |= ENTRYH_FLAG_PS;
    if(prot.write) entry |= ENTRY_FLAG_RW;
    if(!prot.exec) entry |= ENTRY_FLAG_NX;
    if(global) entry |= ENTRY_FLAG_GLOBAL;
    __atomic_store(&current_table[VADDR_TO_INDEX(vaddr, lowest_index)], &entry, __ATOMIC_SEQ_CST);
}

vm_address_space_t *arch_ptm_address_space_create() {
    x86_64_ptm_address_space_t *address_space = heap_alloc(sizeof(x86_64_ptm_address_space_t));
    address_space->pt_top = alloc_page();
    address_space->pt_lock = SPINLOCK_INIT;
    address_space->common.lock = SPINLOCK_INIT;
    address_space->common.regions = vm_create_regions();
    address_space->common.start = USERSPACE_START;
    address_space->common.end = USERSPACE_END;

    memcpy((void *) HHDM(address_space->pt_top + 256 * sizeof(uint64_t)), (void *) HHDM(X86_64_PTM_AS(g_vm_global_address_space)->pt_top + 256 * sizeof(uint64_t)), 256 * sizeof(uint64_t));

    return &address_space->common;
}

void arch_ptm_load_address_space(vm_address_space_t *address_space) {
    x86_64_cr3_write(X86_64_PTM_AS(address_space)->pt_top);
}

void arch_ptm_map(vm_address_space_t *address_space, uintptr_t vaddr, uintptr_t paddr, size_t length, vm_protection_t prot, vm_cache_t cache, vm_privilege_t privilege, bool global) {
    ASSERT(vaddr % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(paddr % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(length % ARCH_PAGE_GRANULARITY == 0);

    if(!prot.read) log(LOG_LEVEL_ERROR, "PTM", "No-read mapping is not supported on x86_64");
    spinlock_acquire_nodw(&X86_64_PTM_AS(address_space)->pt_lock);

    for(size_t i = 0; i < length;) {
        page_size_t cursize = PAGE_SIZE_4K;
        if(paddr % PAGE_SIZE_2M == 0 && vaddr % PAGE_SIZE_2M == 0 && length - i >= PAGE_SIZE_2M) cursize = PAGE_SIZE_2M;

        // TODO: 1gb pages (needs the cpuid check)
        bool g_x86_64_cpu_pdpe1gb_support = true;
        if(g_x86_64_cpu_pdpe1gb_support && paddr % PAGE_SIZE_1G == 0 && vaddr % PAGE_SIZE_1G == 0 && length - i >= PAGE_SIZE_1G) cursize = PAGE_SIZE_1G;

        map_page((uint64_t *) HHDM(X86_64_PTM_AS(address_space)->pt_top), vaddr + i, paddr + i, cursize, prot, cache, privilege, global);

        i += cursize;
    }

    x86_64_tlb_shootdown(vaddr, length);

    spinlock_release_nodw(&X86_64_PTM_AS(address_space)->pt_lock);
}

void arch_ptm_rewrite(vm_address_space_t *address_space, uintptr_t vaddr, size_t length, vm_protection_t prot, vm_cache_t cache, vm_privilege_t privilege, bool global) {
    ASSERT(vaddr % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(length % ARCH_PAGE_GRANULARITY == 0);

    if(!prot.read) log(LOG_LEVEL_ERROR, "PTM", "No-read mapping is not supported on x86_64");
    spinlock_acquire_nodw(&X86_64_PTM_AS(address_space)->pt_lock);

    for(size_t i = 0; i < length;) {
        uint64_t *current_table = (uint64_t *) HHDM(X86_64_PTM_AS(address_space)->pt_top);

        int j = LEVEL_COUNT;
        for(; j > 1; j--) {
            int index = VADDR_TO_INDEX(vaddr + i, j);
            uint64_t entry = current_table[index];

            if((entry & ENTRY_FLAG_PRESENT) == 0) goto skip;
            if((entry & ENTRYH_FLAG_PS) != 0) {
                ASSERT(j <= 3);
                if(INDEX_TO_VADDR(index, j) < vaddr + i || LEVEL_TO_PAGESIZE(j) > length - i) {
                    entry = break_big(current_table, index, j);
                } else {
                    break;
                }
            }

            if(prot.write) entry |= ENTRY_FLAG_RW;
            if(prot.exec) entry &= ~ENTRY_FLAG_NX;
            current_table = (uint64_t *) HHDM(current_table[index] & ENTRYL_ADDRESS_MASK);
        }

        int index = VADDR_TO_INDEX(vaddr + i, j);
        uint64_t entry = current_table[index] | privilege_to_x86_flags(privilege) | cache_to_x86_flags(cache, j == 0 ? PAGE_SIZE_4K : (j == 1 ? PAGE_SIZE_2M : PAGE_SIZE_1G));

        if(prot.write)
            entry |= ENTRY_FLAG_RW;
        else
            entry &= ~ENTRY_FLAG_RW;

        if(!prot.exec)
            entry |= ENTRY_FLAG_NX;
        else
            entry &= ~ENTRY_FLAG_NX;

        if(global)
            entry |= ENTRY_FLAG_GLOBAL;
        else
            entry &= ~ENTRY_FLAG_GLOBAL;

        __atomic_store_n(&current_table[index], entry, __ATOMIC_SEQ_CST);

    skip:
        i += LEVEL_TO_PAGESIZE(j);
    }

    x86_64_tlb_shootdown(vaddr, length);

    spinlock_release_nodw(&X86_64_PTM_AS(address_space)->pt_lock);
}

void arch_ptm_unmap(vm_address_space_t *address_space, uintptr_t vaddr, size_t length) {
    ASSERT(vaddr % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(length % ARCH_PAGE_GRANULARITY == 0);

    spinlock_acquire_nodw(&X86_64_PTM_AS(address_space)->pt_lock);

    for(size_t i = 0; i < length;) {
        uint64_t *current_table = (uint64_t *) HHDM(X86_64_PTM_AS(address_space)->pt_top);

        int j = LEVEL_COUNT;
        for(; j > 1; j--) {
            int index = VADDR_TO_INDEX(vaddr + i, j);
            uint64_t entry = current_table[index];

            if((entry & ENTRY_FLAG_PRESENT) == 0) goto skip;
            if((entry & ENTRYH_FLAG_PS) != 0) {
                ASSERT(j <= 3);
                if(INDEX_TO_VADDR(index, j) < vaddr + i || LEVEL_TO_PAGESIZE(j) > length - i) {
                    entry = break_big(current_table, index, j);
                } else {
                    break;
                }
            }
            current_table = (uint64_t *) HHDM(entry & ENTRYL_ADDRESS_MASK);
        }
        __atomic_store_n(&current_table[VADDR_TO_INDEX(vaddr + i, j)], 0, __ATOMIC_SEQ_CST);

    skip:
        i += LEVEL_TO_PAGESIZE(j);
    }

    x86_64_tlb_shootdown(vaddr, length);

    spinlock_release_nodw(&X86_64_PTM_AS(address_space)->pt_lock);
}

bool arch_ptm_physical(vm_address_space_t *address_space, uintptr_t vaddr, PARAM_OUT(uintptr_t *) paddr) {
    spinlock_acquire_nodw(&X86_64_PTM_AS(address_space)->pt_lock);

    uint64_t *current_table = (uint64_t *) HHDM(X86_64_PTM_AS(address_space)->pt_top);
    int j = LEVEL_COUNT;
    for(; j > 1; j--) {
        int index = VADDR_TO_INDEX(vaddr, j);
        if((current_table[index] & ENTRY_FLAG_PRESENT) == 0) {
            spinlock_release_nodw(&X86_64_PTM_AS(address_space)->pt_lock);
            return false;
        }
        if((current_table[index] & ENTRYH_FLAG_PS) != 0) break;
        current_table = (uint64_t *) HHDM(current_table[index] & ENTRYL_ADDRESS_MASK);
    }

    uint64_t entry = current_table[VADDR_TO_INDEX(vaddr, j)];
    spinlock_release_nodw(&X86_64_PTM_AS(address_space)->pt_lock);
    if((entry & ENTRY_FLAG_PRESENT) == 0) return false;

    switch(j) {
        case 1:  *paddr = ((entry & ENTRYL_ADDRESS_MASK) + (vaddr & (~0x000F'FFFF'FFFF'F000))); break;
        case 2:  *paddr = ((entry & ENTRYH_ADDRESS_MASK) + (vaddr & (~0x000F'FFFF'FFE0'0000))); break;
        case 3:  *paddr = ((entry & ENTRYH_ADDRESS_MASK) + (vaddr & (~0x000F'FFFF'C000'0000))); break;
        default: ASSERT_UNREACHABLE();
    }
    return true;
}

void x86_64_ptm_page_fault_handler(arch_interrupt_frame_t *frame) {
    vm_fault_t fault = VM_FAULT_UNKNOWN;
    if((frame->err_code & PAGEFAULT_FLAG_PRESENT) == 0) fault = VM_FAULT_NOT_PRESENT;

    if(ARCH_CPU_CURRENT_READ(flags.threaded) && vm_fault(x86_64_cr2_read(), fault)) return;
    x86_64_exception_unhandled(frame);
}

static void ptm_init() {
    g_global_address_space.common.lock = SPINLOCK_INIT;
    g_global_address_space.common.regions = vm_create_regions();
    g_global_address_space.common.start = KERNELSPACE_START;
    g_global_address_space.common.end = KERNELSPACE_END;
    g_global_address_space.pt_top = alloc_page();
    g_global_address_space.pt_lock = SPINLOCK_INIT;

    uint64_t *pml4 = (uint64_t *) HHDM(g_global_address_space.pt_top);
    for(int i = 256; i < 512; i++) {
        // Needs to be completely unrestricted as these are not synchronized across address spaces
        pml4[i] = ENTRY_FLAG_PRESENT | ENTRY_FLAG_RW | (alloc_page() & ENTRYL_ADDRESS_MASK);
    }

    g_vm_global_address_space = &g_global_address_space.common;

    x86_64_interrupt_set(0xE, x86_64_ptm_page_fault_handler);
}

INIT_TARGET(ptm, INIT_STAGE_EARLY, ptm_init);
