#include "x86_64/dev/hpet.h"

#include "common/log.h"
#include "memory/mmio.h"

#define GCIDR 0 /* General Capability and ID Register */
#define GCR 2 /* General Configuration Register */
#define GCR_ENABLED (1 << 0)
#define GCR_LEGACY_ROUTING (1 << 1)
#define MCR 30 /* Main Counter Register */

static void *g_hpet;
static uint32_t g_period;

static uint64_t hpet_read(uint64_t reg) {
    return mmio_read64(g_hpet + (reg * sizeof(uint64_t)));
}

static void hpet_write(uint64_t reg, uint64_t value) {
    return mmio_write64(g_hpet + (reg * sizeof(uint64_t)), value);
}

time_t hpet_current_time() {
    return (hpet_read(MCR) * g_period) / (TIME_FEMTOSECONDS_IN_SECOND / TIME_NANOSECONDS_IN_SECOND);
}

void x86_64_hpet_init(struct acpi_hpet *header) {
    g_hpet = mmio_map(header->address.address, 1024); // TODO: Assuming a lot about ACPI address
    g_period = (uint32_t) (hpet_read(GCIDR) >> 32);

    hpet_write(GCR, hpet_read(GCR) & ~(GCR_LEGACY_ROUTING));
    hpet_write(GCR, hpet_read(GCR) | GCR_ENABLED);

    log(LOG_LEVEL_DEBUG, "HPET", "Initialized, frequency: %lu", TIME_FEMTOSECONDS_IN_SECOND / g_period);
}
