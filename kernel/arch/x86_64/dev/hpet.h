#pragma once

#include "dev/acpi/acpi.h"
#include "sys/time.h"

/// Initialize HPET.
/// @param header "HPET" header
void x86_64_hpet_init(acpi_sdt_header_t *header);

/// Get the current time using the HPET MCR.
time_t hpet_current_time();
