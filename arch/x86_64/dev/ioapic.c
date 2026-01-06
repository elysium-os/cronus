#include "x86_64/dev/ioapic.h"

#include "arch/mmio.h"
#include "common/log.h"
#include "memory/mmio.h"

#include <uacpi/acpi.h>

#define VER 0x1
#define VER_MAX_REDIRECTION_ENTRY(VALUE) (((VALUE) >> 16) & 0xFF)

#define IOREDX(X) (0x10 + (X) * 2)
#define IOREDX_DELMOD(VALUE) (((VALUE) & 0x7) << 8)
#define IOREDX_DESTMOD (1 << 11)
#define IOREDX_INTPOL (1 << 13)
#define IOREDX_TRIGGERMODE (1 << 15)
#define IOREDX_MASK (1 << 16)

#define LEGACY_POLARITY (0b11)
#define LEGACY_TRIGGER (0b11 << 2)
#define LEGACY_POLARITY_HIGH 0b1
#define LEGACY_POLARITY_LOW 0b11
#define LEGACY_TRIGGER_EDGE (0b1 << 2)
#define LEGACY_TRIGGER_LEVEL (0b11 << 2)

typedef struct {
    uint8_t gsi;
    uint16_t flags;
} legacy_irq_translation_t;

static legacy_irq_translation_t g_legacy_irq_map[16] = {
    { 0,  0 },
    { 1,  0 },
    { 2,  0 },
    { 3,  0 },
    { 4,  0 },
    { 5,  0 },
    { 6,  0 },
    { 7,  0 },
    { 8,  0 },
    { 9,  0 },
    { 10, 0 },
    { 11, 0 },
    { 12, 0 },
    { 13, 0 },
    { 14, 0 },
    { 15, 0 }
};

static volatile uint32_t *g_ioapic;

static void ioapic_write(uint32_t index, uint32_t data) {
    arch_mmio_write32(g_ioapic, index & 0xFF);
    arch_mmio_write32(&g_ioapic[4], data);
}

static uint32_t ioapic_read(uint32_t index) {
    arch_mmio_write32(g_ioapic, index & 0xFF);
    return arch_mmio_read32(&g_ioapic[4]);
}

void x86_64_ioapic_init(struct acpi_madt *apic_table) {
    uintptr_t ioapic_address = 0;

    uint32_t nbytes = apic_table->hdr.length - offsetof(struct acpi_madt, entries);
    struct acpi_entry_hdr *current_record = apic_table->entries; // (madt_record_t *) ((uintptr_t) apic_table->virt_addr + sizeof(struct acpi_sdt_hdr) + sizeof(madt_header_t));
    while(nbytes > 0) {
        switch(current_record->type) {
            case ACPI_MADT_ENTRY_TYPE_IOAPIC: ioapic_address = ((struct acpi_madt_ioapic *) current_record)->address; break;
            case ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE:
                struct acpi_madt_interrupt_source_override *override_record = (struct acpi_madt_interrupt_source_override *) current_record;
                g_legacy_irq_map[override_record->source].gsi = override_record->gsi;
                g_legacy_irq_map[override_record->source].flags = override_record->flags;
                break;
        }
        nbytes -= current_record->length;
        current_record = (struct acpi_entry_hdr *) ((uintptr_t) current_record + current_record->length);
    }

    log(LOG_LEVEL_DEBUG, "IOAPIC", "Found ioapic at %#lx", ioapic_address);

    g_ioapic = mmio_map(ioapic_address, 4096);
}

void x86_64_ioapic_map_gsi(uint8_t gsi, uint8_t lapic_id, bool low_polarity, bool trigger_mode, uint8_t vector) {
    uint32_t iored_low = IOREDX(gsi);

    uint32_t low_entry = vector;
    if(low_polarity) low_entry |= IOREDX_INTPOL;
    if(!trigger_mode) low_entry |= IOREDX_TRIGGERMODE;
    ioapic_write(iored_low, low_entry);

    uint32_t high_data = ioapic_read(iored_low + 1);
    high_data &= ~((uint32_t) 0xFF << 24);
    high_data |= lapic_id << 24;
    ioapic_write(iored_low + 1, high_data);
}

void x86_64_ioapic_map_legacy_irq(uint8_t irq, uint8_t lapic_id, bool fallback_low_polarity, bool fallback_trigger_mode, uint8_t vector) {
    if(irq < 16) {
        switch(g_legacy_irq_map[irq].flags & LEGACY_POLARITY) {
            case LEGACY_POLARITY_LOW:  fallback_low_polarity = true; break;
            case LEGACY_POLARITY_HIGH: fallback_low_polarity = false; break;
        }
        switch(g_legacy_irq_map[irq].flags & LEGACY_TRIGGER) {
            case LEGACY_TRIGGER_EDGE:  fallback_trigger_mode = true; break;
            case LEGACY_TRIGGER_LEVEL: fallback_trigger_mode = false; break;
        }
        irq = g_legacy_irq_map[irq].gsi;
    }
    x86_64_ioapic_map_gsi(irq, lapic_id, fallback_low_polarity, fallback_trigger_mode, vector);
}
