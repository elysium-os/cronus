#include "acpi.h"

#include "common/assert.h"
#include "lib/mem.h"
#include "memory/hhdm.h"

typedef struct [[gnu::packed]] {
    uint8_t signature[8];
    uint8_t checksum;
    uint8_t oem[6];
    uint8_t revision;
    uint32_t rsdt_address;
} acpi_rsdp_t;

typedef struct [[gnu::packed]] {
    acpi_rsdp_t rsdp;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} acpi_rsdp_ext_t;

static uintptr_t g_rsdp;
static acpi_sdt_header_t *g_xsdt, *g_rsdt;

static uint8_t checksum(uint8_t *src, uint32_t size) {
    uint32_t checksum = 0;
    for(uint8_t i = 0; i < size; i++) checksum += src[i];
    return (checksum & 1) == 0;
}

void acpi_init(uintptr_t rsdp) {
    g_rsdp = rsdp;
    if(acpi_revision() > 0) {
        acpi_rsdp_ext_t *rsdp_ext = (acpi_rsdp_ext_t *) HHDM(rsdp);
        ASSERT(checksum(((uint8_t *) rsdp_ext) + sizeof(acpi_rsdp_t), sizeof(acpi_rsdp_ext_t) - sizeof(acpi_rsdp_t)));
        g_xsdt = (acpi_sdt_header_t *) HHDM(rsdp_ext->xsdt_address);
    } else {
        g_rsdt = (acpi_sdt_header_t *) HHDM(((acpi_rsdp_t *) HHDM(rsdp))->rsdt_address);
    }
}

acpi_sdt_header_t *acpi_find_table(uint8_t *signature) {
    if(g_xsdt != NULL) {
        uint32_t entry_count = (g_xsdt->length - sizeof(acpi_sdt_header_t)) / sizeof(uint64_t);
        uint64_t *entry_ptr = (uint64_t *) ((uintptr_t) g_xsdt + sizeof(acpi_sdt_header_t));
        for(uint32_t i = 0; i < entry_count; i++) {
            acpi_sdt_header_t *entry = (acpi_sdt_header_t *) HHDM(*entry_ptr);
            if(memcmp(entry->signature, signature, 4) == 0) return entry;
            entry_ptr++;
        }
    } else if(g_rsdt != NULL) {
        uint32_t entry_count = (g_rsdt->length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);
        uint32_t *entry_ptr = (uint32_t *) ((uintptr_t) g_rsdt + sizeof(acpi_sdt_header_t));
        for(uint32_t i = 0; i < entry_count; i++) {
            acpi_sdt_header_t *entry = (acpi_sdt_header_t *) HHDM(*entry_ptr);
            if(memcmp(entry->signature, signature, 4) == 0) return entry;
            entry_ptr++;
        }
    }
    return NULL;
}

uint8_t acpi_revision() {
    return ((acpi_rsdp_t *) HHDM(g_rsdp))->revision;
}

uintptr_t acpi_rsdp() {
    return g_rsdp;
}
