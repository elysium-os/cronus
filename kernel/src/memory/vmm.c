#include "vmm.h"
#include <stdbool.h>
#include <string.h>
#include <memory/pmm.h>
#include <memory/hhdm.h>
#include <cpu/msr.h>

#define DEFAULT_FLAGS VMM_FLAG_READWRITE

static vmm_page_table_t *g_pml4;

inline static void pt_set_address(uint64_t *entry, uintptr_t address) {
    address &= 0x000FFFFFFFFFF000;
    *entry &= 0xFFF0000000000FFF;
    *entry |= address;
}

inline static uintptr_t pt_get_address(uint64_t entry) {
    return entry & 0x000FFFFFFFFFF000;
}

inline static bool pt_get_flag(uint64_t entry, vmm_pt_flag_t flag) {
    return entry & flag;
}

inline static uint64_t address_to_index(uintptr_t address, uint8_t level) {
    return (address >> (3 + (4 - level) * 9)) & 0x1FF;
}

inline static void load_cr3(uint64_t value) {
    asm volatile("movq %0, %%cr3" : : "r" (value) : "memory");
}

inline static uint64_t read_cr3() {
    uint64_t value;
    asm volatile("movq %%cr3, %0" : "=r" (value));
    return value;
}

void vmm_initialize(uintptr_t pml4_address) {
    g_pml4 = (vmm_page_table_t *) HHDM(pml4_address);

    uint64_t pat = msr_get(MSR_PAT);
    pat &= ~(((uint64_t) 0b111 << 48) | ((uint64_t) 0b111 << 40));
    pat |= ((uint64_t) 0x1 << 48) | ((uint64_t) 0x5 << 40);
    msr_set(MSR_PAT, pat);

    for(int i = 0; i < 256; i++) {
        g_pml4->entries[i] = 0;
    }

    load_cr3(read_cr3());
}

uintptr_t vmm_physical(void *virtual_address) {
    vmm_page_table_t *current_table = g_pml4;
    for(uint8_t i = 0; i < 3; i++) {
        uint64_t entry = current_table->entries[address_to_index((uintptr_t) virtual_address, i)];
        if(!pt_get_flag(entry, VMM_PT_FLAG_PRESENT)) return 0;
        current_table = (vmm_page_table_t *) HHDM(pt_get_address(entry));
    }
    uint64_t entry = current_table->entries[address_to_index((uintptr_t) virtual_address, 3)];
    if(!pt_get_flag(entry, VMM_PT_FLAG_PRESENT)) return 0;
    return pt_get_address(entry);
}

void vmm_mapf(void *physical_address, void *virtual_address, uint64_t flags) {
    vmm_page_table_t *current_table = g_pml4;
    for(uint8_t i = 0; i < 3; i++) {
        uint64_t index = address_to_index((uintptr_t) virtual_address, i);
        uint64_t entry = current_table->entries[index];
        if(!pt_get_flag(entry, VMM_PT_FLAG_PRESENT)) {
            uintptr_t free_address = (uintptr_t) pmm_page_request();
            vmm_page_table_t *new_table = (vmm_page_table_t *) HHDM(free_address);
            memset(new_table, 0, 0x1000);

            uint64_t new_entry = 0;
            pt_set_address(&new_entry, free_address);
            new_entry |= VMM_PT_FLAG_PRESENT;
            if(flags & VMM_FLAG_READWRITE) new_entry |= VMM_PT_FLAG_READWRITE;
            if(flags & VMM_FLAG_USER) new_entry |= VMM_PT_FLAG_USER;
            current_table->entries[index] = new_entry;
            current_table = new_table;
        } else {
            if(flags & VMM_FLAG_READWRITE && !(entry & VMM_PT_FLAG_READWRITE)) current_table->entries[index] |= VMM_PT_FLAG_READWRITE;
            if(flags & VMM_FLAG_USER && !(entry & VMM_PT_FLAG_USER)) current_table->entries[index] |= VMM_PT_FLAG_USER;
            current_table = (vmm_page_table_t *) HHDM(pt_get_address(entry));
        }
    }

    uint64_t entry = 0;
    pt_set_address(&entry, (uintptr_t) physical_address);
    entry |= VMM_PT_FLAG_PRESENT;
    if(flags & VMM_FLAG_READWRITE) entry |= VMM_PT_FLAG_READWRITE;
    if(flags & VMM_FLAG_USER) entry |= VMM_PT_FLAG_USER;
    current_table->entries[address_to_index((uintptr_t) virtual_address, 3)] = entry;
}

void vmm_map(void *physical_address, void *virtual_address) {
    vmm_mapf(physical_address, virtual_address, DEFAULT_FLAGS);
}

void vmm_dbg_tables(uint64_t indexes[4], uint64_t entries[4]) {
    vmm_page_table_t *current_table = g_pml4;
    for(uint8_t i = 0; i < 3; i++) {
        entries[i] = current_table->entries[indexes[i]];
        if(!pt_get_flag(entries[i], VMM_PT_FLAG_PRESENT)) return;
        current_table = (vmm_page_table_t *) HHDM(pt_get_address(entries[i]));
    }
    entries[3] = current_table->entries[indexes[3]];
}