#include "hpet.h"

#include "memory/hhdm.h"
#include "sys/time.h"

#define GCIDR 0 /* General Capability and ID Register */
#define GCIDR_TIMER_COUNT(GCIDR) (((GCIDR) >> 8) & 0b11111)
#define GCIDR_COUNTER_PERIOD(GCIDR) ((uint32_t) ((GCIDR) >> 32))

#define GCR 2 /* General Configuration Register */
#define GCR_ENABLED (1 << 0)
#define GCR_LEGACY_ROUTING (1 << 1)

#define MCR 30 /* Main Counter Register */

#define CCR(N) (32 + 4 * (N)) /* Timer N Configuration and Capabilities Register */
#define CCR_INTERRUPTS_ENABLED (1 << 2)
#define CCR_PERIODIC (1 << 3)
#define CCR_PERIODIC_CAPABLE (1 << 4)
#define CCR_64BIT_CAPABLE (1 << 5)
#define CCR_TIMER_VALUE_SET (1 << 6)
#define CCR_INTERRUPT_MASK(CCR) ((uint32_t) ((CCR) >> 32))

#define CVR(N) (33 + 4 * (N)) /* Timer N Comparator Register */

#define FREQUENCY(HPET) (TIME_FEMTOSECONDS_IN_SECOND / GCIDR_COUNTER_PERIOD(HPET[GCIDR])) /* Hz*/

typedef struct [[gnu::packed]] {
    acpi_sdt_header_t sdt_header;
    uint32_t event_timer_block_id;
    acpi_generic_address_structure_t address;
    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
} hpet_header_t;

static volatile uint64_t *g_hpet;
static uint16_t g_min_periodic_timer_length;
static uint64_t g_period;

static time_t hpet_current_time();

time_source_t g_hpet_time_source = {
    .name = "HPET",
    .resolution = TIME_NANOSECONDS_IN_SECOND,
    .current = hpet_current_time,
};

static time_t hpet_current_time() {
    return g_hpet[MCR] * g_period;
}

void x86_64_hpet_init(acpi_sdt_header_t *header) {
    hpet_header_t *hpet_header = (hpet_header_t *) header;
    g_hpet = (uint64_t *) HHDM(hpet_header->address.address); // TODO: Assuming a lot about ACPI address
    g_min_periodic_timer_length = hpet_header->minimum_tick;
    g_period = GCIDR_COUNTER_PERIOD(g_hpet[GCIDR]) / 1'000'000; // convert to nanoseconds from femto
    g_hpet[GCR] &= ~(GCR_LEGACY_ROUTING);
    g_hpet[GCR] |= GCR_ENABLED;
}
