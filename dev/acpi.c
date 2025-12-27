#include "dev/acpi.h"

#include "common/log.h"
#include "common/panic.h"
#include "init.h"
#include "sys/init.h"

#include <uacpi/uacpi.h>

uintptr_t g_acpi_rsdp = 0;

INIT_TARGET(rsdp, INIT_PROVIDES("rsdp"), INIT_DEPS("log")) {
    g_acpi_rsdp = g_init_boot_info->acpi_rsdp_address;
    log(LOG_LEVEL_DEBUG, "INIT", "RSDP at %#lx", g_acpi_rsdp);
}

INIT_TARGET(uacpi, INIT_PROVIDES("acpi_tables"), INIT_DEPS("rsdp", "spinlock", "heap", "log", "vm")) {
    uacpi_status ret = uacpi_initialize(0);
    if(uacpi_unlikely_error(ret)) panic("INIT", "UACPI initialization failed (%s)", uacpi_status_to_string(ret));
};
