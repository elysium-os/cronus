#include "acpi.h"

#include "arch/mmio.h"
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
static void *g_xsdt, *g_rsdt;

static uint8_t checksum(uint8_t *src, uint32_t size) {
    uint32_t checksum = 0;
    for(uint8_t i = 0; i < size; i++) checksum += mmio_read8(&src[i]);
    return (checksum & 1) == 0;
}

void acpi_init(uintptr_t rsdp) {
    g_rsdp = rsdp;
    if(acpi_revision() > 0) {
        ASSERT(checksum(((uint8_t *) HHDM(rsdp)) + sizeof(acpi_rsdp_t), sizeof(acpi_rsdp_ext_t) - sizeof(acpi_rsdp_t)));

        g_xsdt = (acpi_sdt_header_t *) HHDM(mmio_read64((void *) (HHDM(rsdp) + offsetof(acpi_rsdp_ext_t, xsdt_address))));
    } else {
        g_rsdt = (acpi_sdt_header_t *) HHDM(mmio_read64((void *) (HHDM(rsdp) + offsetof(acpi_rsdp_t, rsdt_address))));
    }
}

acpi_sdt_header_t *acpi_find_table(uint8_t *signature) {
    if(g_xsdt != nullptr) {
        uint32_t entry_count = (mmio_read32((void *) ((uintptr_t) g_xsdt + offsetof(acpi_rsdp_ext_t, length))) - sizeof(acpi_sdt_header_t)) / sizeof(uint64_t);
        uint64_t *entry_ptr = (uint64_t *) ((uintptr_t) g_xsdt + sizeof(acpi_sdt_header_t));
        for(uint32_t i = 0; i < entry_count; i++) {
            acpi_sdt_header_t *entry = (acpi_sdt_header_t *) HHDM(mmio_read64(entry_ptr));
            uint32_t entry_signature = mmio_read32(&entry->signature);
            if(memcmp(&entry_signature, signature, 4) == 0) return entry;
            entry_ptr++;
        }
    } else if(g_rsdt != nullptr) {
        uint32_t entry_count = (mmio_read32((void *) ((uintptr_t) g_rsdt + offsetof(acpi_rsdp_ext_t, length))) - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);
        uint32_t *entry_ptr = (uint32_t *) ((uintptr_t) g_rsdt + sizeof(acpi_sdt_header_t));
        for(uint32_t i = 0; i < entry_count; i++) {
            acpi_sdt_header_t *entry = (acpi_sdt_header_t *) HHDM(mmio_read32(entry_ptr));
            uint32_t entry_signature = mmio_read32(&entry->signature);
            if(memcmp(&entry_signature, signature, 4) == 0) return entry;
            entry_ptr++;
        }
    }
    return nullptr;
}

uint8_t acpi_revision() {
    return mmio_read8((void *) (HHDM(g_rsdp) + offsetof(acpi_rsdp_t, revision)));
}

uintptr_t acpi_rsdp() {
    return g_rsdp;
}
