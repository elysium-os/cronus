#pragma once

#include "dev/acpi/acpi.h"
#include "sys/time.h"

extern time_source_t g_hpet_time_source;

/// Initialize HPET.
/// @param Header HPET header
void x86_64_hpet_init(acpi_sdt_header_t *header);
