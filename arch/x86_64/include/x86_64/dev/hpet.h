#pragma once

#include "sys/time.h"

#include <uacpi/acpi.h>

extern bool g_hpet_initialized;

/// Get the current time using the HPET MCR.
time_t hpet_current_time();
