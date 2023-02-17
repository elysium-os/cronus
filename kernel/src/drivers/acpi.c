#include "acpi.h"
#include <stdbool.h>
#include <string.h>
#include <panic.h>
#include <memory/hhdm.h>
#include <memory/vmm.h>

#define RSDP_REGION_ONE_BASE 0x80000
#define RSDP_REGION_ONE_END 0x9FFFF
#define RSDP_REGION_TWO_BASE 0xE0000
#define RSDP_REGION_TWO_END 0xFFFFF
#define RSDP_IDENTIFIER 0x2052545020445352 // "RSDP PTR "

static acpi_sdt_header_t *g_sdts;
static uint8_t g_revision;

static uint8_t checksum(uint8_t *src, uint32_t size) {
    uint32_t checksum = 0;
    for(uint8_t i = 0; i < size; i++) checksum += src[i];
    return (checksum & 1) == 0;
}

static uintptr_t scan_region(uintptr_t start, uintptr_t end) {
    for(uintptr_t page = start & ~0xFFF; page <= end; page += 0x1000) {
        vmm_map((void *) page, (void *) (g_hhdm_address + page));
    }
    uintptr_t addr = HHDM(start);
    end = HHDM(end);
    while(addr < end) {
        if(*(uintptr_t *) addr == RSDP_IDENTIFIER) {
            if(checksum((uint8_t *) addr, sizeof(acpi_rsdp_t))) {
                return addr;
            }
        }
        addr += 16;
    }
    return 0;
}

void acpi_initialize() {
    uintptr_t address = scan_region(RSDP_REGION_ONE_BASE, RSDP_REGION_ONE_END);
    if(!address) address = scan_region(RSDP_REGION_TWO_BASE, RSDP_REGION_TWO_END);
    if(!address) panic("ACPI", "Failed to locate the RSDP");

    acpi_rsdp_t *rsdp = (acpi_rsdp_t *) address;
    g_revision = rsdp->revision;
    if(rsdp->revision > 0) {
        acpi_rsdp_ext_t *rsdp_ext = (acpi_rsdp_ext_t *) address;
        if(!checksum(((uint8_t *) rsdp_ext) + sizeof(acpi_rsdp_t), sizeof(acpi_rsdp_ext_t) - sizeof(acpi_rsdp_t))) panic("ACPI", "Checksum for Extended RSDP failed");
        g_sdts = (acpi_sdt_header_t *) HHDM(rsdp_ext->xsdt_address);
    } else {
        g_sdts = (acpi_sdt_header_t *) HHDM(rsdp->rsdt_address);
    }
}

acpi_sdt_header_t *acpi_find_table(uint8_t *signature) {
    uint32_t entries = (g_sdts->length - sizeof(acpi_sdt_header_t)) / 4;
    uint32_t *entry_ptr = (uint32_t *) ((uintptr_t) g_sdts + sizeof(acpi_sdt_header_t));
    for(uint32_t i = 0; i < entries; i++) {
        acpi_sdt_header_t *entry = (acpi_sdt_header_t *) HHDM(*entry_ptr);
        if(!memcmp(entry->signature, signature, 4)) return entry;
        entry_ptr++;
    }
    return 0;
}

uint8_t acpi_revision() {
    return g_revision;
}