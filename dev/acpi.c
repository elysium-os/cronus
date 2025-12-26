#include "dev/acpi.h"

#include "common/panic.h"
#include "sys/init.h"

#include <uacpi/uacpi.h>

uintptr_t g_acpi_rsdp = 0;

// TODO: ioapic is x86. we need to not depend on it within uacpi
INIT_TARGET(uacpi, INIT_PROVIDES("uacpi", "acpi_tables"), INIT_DEPS("rsdp", "spinlock", "heap", "log", "vm", "time")) {
    uacpi_status ret = uacpi_initialize(0);
    if(uacpi_unlikely_error(ret)) panic("INIT", "UACPI initialization failed (%s)", uacpi_status_to_string(ret));
};
