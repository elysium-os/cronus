#pragma once

#include "sys/time.h"

#include <uacpi/acpi.h>

/// Initialize HPET.
void x86_64_hpet_init(struct acpi_hpet *header);

/// Get the current time using the HPET MCR.
time_t hpet_current_time();
