#include "dev/acpi.h"

#include "common/panic.h"
#include "sys/init.h"

#include <uacpi/uacpi.h>

uintptr_t g_acpi_rsdp = 0;

static void init_acpi_tables() {
    uacpi_status ret = uacpi_initialize(0);
    if(uacpi_unlikely_error(ret)) panic("INIT", "UACPI initialization failed (%s)", uacpi_status_to_string(ret));
}

INIT_TARGET(acpi_tables, INIT_STAGE_BEFORE_DEV, init_acpi_tables);
