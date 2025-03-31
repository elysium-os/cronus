#include "ioapic.h"

#include <memory/hhdm.h>

#define VER 0x1
#define VER_MAX_REDIRECTION_ENTRY(val) (((val) >> 16) & 0xFF)

#define IOREDx(x) (0x10 + (x) * 2)
#define IOREDx_DELMOD(val) (((val) & 0x7) << 8)
#define IOREDx_DESTMOD (1 << 11)
#define IOREDx_INTPOL (1 << 13)
#define IOREDx_TRIGGERMODE (1 << 15)
#define IOREDx_MASK (1 << 16)

#define LEGACY_POLARITY (0b11)
#define LEGACY_TRIGGER (0b11 << 2)
#define LEGACY_POLARITY_HIGH 0b1
#define LEGACY_POLARITY_LOW 0b11
#define LEGACY_TRIGGER_EDGE (0b1 << 2)
#define LEGACY_TRIGGER_LEVEL (0b11 << 2)

typedef enum {
    MADT_RECORD_TYPE_LAPIC = 0,
    MADT_RECORD_TYPE_IOAPIC,
    MADT_RECORD_TYPE_SOURCE_OVERRIDE,
    MADT_RECORD_TYPE_NMI_SOURCE,
    MADT_RECORD_TYPE_NMI,
    MADT_RECORD_TYPE_LAPIC_ADDRESS,
    MADT_RECORD_TYPE_LX2APIC = 9
} madt_record_type_t;

typedef struct [[gnu::packed]] {
    acpi_sdt_header_t sdt_header;
    uint32_t local_apic_address;
    uint32_t flags;
} madt_header_t;

typedef struct [[gnu::packed]] {
    uint8_t type;
    uint8_t length;
} madt_record_t;

typedef struct [[gnu::packed]] {
    madt_record_t base;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags;
} madt_record_lapic_t;

typedef struct [[gnu::packed]] {
    madt_record_t base;
    uint8_t ioapic_id;
    uint8_t rsv0;
    uint32_t ioapic_address;
    uint32_t global_system_interrupt_base;
} madt_record_ioapic_t;

typedef struct [[gnu::packed]] {
    madt_record_t base;
    uint16_t rsv0;
    uint64_t lapic_address;
} madt_record_lapic_address_t;

typedef struct [[gnu::packed]] {
    madt_record_t base;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t global_system_interrupt;
    uint16_t flags;
} madt_record_source_override_t;

typedef struct [[gnu::packed]] {
    madt_record_ioapic_t base;
    uint8_t acpi_processor_id;
    uint16_t flags;
    uint8_t lint;
} madt_record_nmi_t;

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
    g_ioapic[0] = index & 0xFF;
    g_ioapic[4] = data;
}

static uint32_t ioapic_read(uint32_t index) {
    g_ioapic[0] = index & 0xFF;
    return g_ioapic[4];
}

void x86_64_ioapic_init(acpi_sdt_header_t *apic_header) {
    madt_header_t *madt = (madt_header_t *) apic_header;
    uintptr_t ioapic_address = 0;

    uint32_t nbytes = madt->sdt_header.length - sizeof(madt_header_t);
    madt_record_t *current_record = (madt_record_t *) ((uintptr_t) madt + sizeof(madt_header_t));
    while(nbytes > 0) {
        switch(current_record->type) {
            case MADT_RECORD_TYPE_IOAPIC:
                madt_record_ioapic_t *ioapic_record = (madt_record_ioapic_t *) current_record;
                ioapic_address = ioapic_record->ioapic_address;
                break;
            case MADT_RECORD_TYPE_SOURCE_OVERRIDE:
                madt_record_source_override_t *override_record = (madt_record_source_override_t *) current_record;
                g_legacy_irq_map[override_record->irq_source].gsi = override_record->global_system_interrupt;
                g_legacy_irq_map[override_record->irq_source].flags = override_record->flags;
                break;
        }
        nbytes -= current_record->length;
        current_record = (madt_record_t *) ((uintptr_t) current_record + current_record->length);
    }

    g_ioapic = (uint32_t *) HHDM(ioapic_address);
}

void x86_64_ioapic_map_gsi(uint8_t gsi, uint8_t lapic_id, bool low_polarity, bool trigger_mode, uint8_t vector) {
    uint32_t iored_low = IOREDx(gsi);

    uint32_t low_entry = vector;
    if(low_polarity) low_entry |= IOREDx_INTPOL;
    if(!trigger_mode) low_entry |= IOREDx_TRIGGERMODE;
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
