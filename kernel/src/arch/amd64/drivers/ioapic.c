#include "ioapic.h"
#include <memory/vmm.h>
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

typedef struct {
    acpi_sdt_header_t sdt_header;
    uint32_t local_apic_address;
    uint32_t flags;
} __attribute__((packed)) ioapic_madt_header_t;

typedef enum {
    LAPIC = 0,
    IOAPIC,
    SOURCE_OVERRIDE,
    NMI_SOURCE,
    NMI,
    LAPIC_ADDRESS,
    LX2APIC = 9
} ioapic_madt_record_type_t;

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) ioapic_madt_record_t;

typedef struct {
    ioapic_madt_record_t base;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed)) ioapic_madt_record_lapic_t;

typedef struct {
    ioapic_madt_record_t base;
    uint8_t ioapic_id;
    uint8_t rsv0;
    uint32_t ioapic_address;
    uint32_t global_system_interrupt_base;
} __attribute__((packed)) ioapic_madt_record_ioapic_t;

typedef struct {
    ioapic_madt_record_t base;
    uint16_t rsv0;
    uint64_t lapic_address;
} __attribute__((packed)) ioapic_madt_record_lapic_address_t;

typedef struct {
    ioapic_madt_record_t base;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t global_system_interrupt;
    uint16_t flags;
} __attribute__((packed)) ioapic_madt_record_source_override_t;

typedef struct {
    ioapic_madt_record_ioapic_t base;
    uint8_t acpi_processor_id;
    uint16_t flags;
    uint8_t lint;
} __attribute__((packed)) ioapic_madt_record_nmi_t;

typedef struct {
    uint8_t gsi;
    uint16_t flags;
} ioapic_legacy_irq_translation_t;

static volatile uint32_t *g_ioapic;

static ioapic_legacy_irq_translation_t g_legacy_irq_map[16] = {
    {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0},
    {6, 0}, {7, 0}, {8, 0}, {9, 0}, {10, 0},
    {11, 0}, {12, 0}, {13, 0}, {14, 0}, {15, 0}
};

static void ioapic_write(uint32_t index, uint32_t data) {
    g_ioapic[0] = index & 0xFF;
    g_ioapic[4] = data;
}

static uint32_t ioapic_read(uint32_t index) {
    g_ioapic[0] = index & 0xFF;
    return g_ioapic[4];
}

void ioapic_initialize(acpi_sdt_header_t *apic_header) {
    ioapic_madt_header_t *madt = (ioapic_madt_header_t *) apic_header;
    uintptr_t ioapic_address = 0;

    uint32_t nbytes = madt->sdt_header.length - sizeof(ioapic_madt_header_t);
    ioapic_madt_record_t *current_record = (ioapic_madt_record_t *) ((uintptr_t) madt + sizeof(ioapic_madt_header_t));
    while(nbytes > 0) {
        switch(current_record->type) {
            case IOAPIC:
                ioapic_madt_record_ioapic_t *ioapic_record = (ioapic_madt_record_ioapic_t *) current_record;
                ioapic_address = ioapic_record->ioapic_address;
                break;
            case SOURCE_OVERRIDE:
                ioapic_madt_record_source_override_t *override_record = (ioapic_madt_record_source_override_t *) current_record;
                g_legacy_irq_map[override_record->irq_source].gsi = override_record->global_system_interrupt;
                g_legacy_irq_map[override_record->irq_source].flags = override_record->flags;
                break;
            case NMI:
                ioapic_madt_record_nmi_t *nmi_record = (ioapic_madt_record_nmi_t *) current_record;
                break;
        }
        nbytes -= current_record->length;
        current_record = (ioapic_madt_record_t *) ((uintptr_t) current_record + current_record->length);
    }

    g_ioapic = (uint32_t *) HHDM(ioapic_address);
}

void ioapic_map_gsi(uint8_t gsi, uint8_t apic_id, bool low_polarity, bool edge, uint8_t vector) {
    uint32_t iored_low = IOREDx(gsi);

    uint32_t low_entry = vector;
    if(low_polarity) low_entry |= IOREDx_INTPOL;
    if(!edge) low_entry |= IOREDx_TRIGGERMODE;
    ioapic_write(iored_low, low_entry);

    uint32_t high_data = ioapic_read(iored_low + 1);
    high_data &= ~(0xFF << 24);
    high_data |= apic_id << 24;
    ioapic_write(iored_low + 1, high_data);
}

void ioapic_map_legacy_irq(uint8_t irq, uint8_t apic_id, bool fallback_low_polarity, bool fallback_edge, uint8_t vector) {
    if(irq < 16) {
        switch(g_legacy_irq_map[irq].flags & LEGACY_POLARITY) {
            case LEGACY_POLARITY_LOW:
                fallback_low_polarity = true;
                break;
            case LEGACY_POLARITY_HIGH:
                fallback_low_polarity = false;
                break;
        }
        switch(g_legacy_irq_map[irq].flags & LEGACY_TRIGGER) {
            case LEGACY_TRIGGER_EDGE:
                fallback_edge = true;
                break;
            case LEGACY_TRIGGER_LEVEL:
                fallback_edge = false;
                break;
        }
        irq = g_legacy_irq_map[irq].gsi;
    }
    ioapic_map_gsi(irq, apic_id, fallback_low_polarity, fallback_edge, vector);
}