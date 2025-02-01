#pragma once

#include "dev/acpi/acpi.h"
#include "sys/time.h"

#include <stdint.h>

extern time_source_t g_hpet_time_source;

/**
 * @brief Initialize HPET.
 * @param header HPET header
 */
void x86_64_hpet_init(acpi_sdt_header_t *header);
