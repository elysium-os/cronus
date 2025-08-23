#pragma once

#include <stdint.h>
#include <uacpi/acpi.h>

/// Initialize IOAPIC.
/// @param apic_table MADT table
void x86_64_ioapic_init(struct acpi_madt *apic_table);

/// Map GSI to interrupt vector.
/// @param gsi Global system interrupt (IRQ)
/// @param lapic_id Local apic ID
/// @param low_polarity true = high active, false = low active
/// @param trigger_mode true = edge sensitive, false = level sensitive
/// @param vector Interrupt vector
void x86_64_ioapic_map_gsi(uint8_t gsi, uint8_t lapic_id, bool low_polarity, bool trigger_mode, uint8_t vector);

/// Map legacy IRQ to interrupt vector.
/// @param irq Legacy IRQ
/// @param lapic_id Local apic ID
/// @param fallback_low_polarity true = high active, false = low active
/// @param fallback_trigger_mode true = edge sensitive, false = level sensitive
/// @param vector Interrupt vector
void x86_64_ioapic_map_legacy_irq(uint8_t irq, uint8_t lapic_id, bool fallback_low_polarity, bool fallback_trigger_mode, uint8_t vector);
