#pragma once

#include "dev/acpi/acpi.h"

#include <stdint.h>

/// Initialize IOAPIC.
/// @param apic_header MADT header
void x86_64_ioapic_init(acpi_sdt_header_t *apic_header);

/// Map GSI to interrupt vector.
/// @param gsi global system interrupt (IRQ)
/// @param lapic_id local apic ID
/// @param low_polarity polarity; true = high active, false = low active
/// @param trigger_mode trigger mode; true = edge sensitive, false = level sensitive
/// @param vector interrupt vector
void x86_64_ioapic_map_gsi(uint8_t gsi, uint8_t lapic_id, bool low_polarity, bool trigger_mode, uint8_t vector);

/// Map legacy IRQ to interrupt vector.
/// @param irq legacy IRQ
/// @param lapic_id local apic ID
/// @param fallback_low_polarity fallback polarity; true = high active, false = low active
/// @param fallback_trigger_mode fallback trigger mode; true = edge sensitive, false = level sensitive
/// @param vector interrupt vector
void x86_64_ioapic_map_legacy_irq(uint8_t irq, uint8_t lapic_id, bool fallback_low_polarity, bool fallback_trigger_mode, uint8_t vector);
