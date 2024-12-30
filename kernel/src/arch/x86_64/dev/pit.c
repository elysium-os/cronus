#include "pit.h"

#include "arch/x86_64/cpu/port.h"

#define CHANNEL0_DATA 0x40
#define CHANNEL1_DATA 0x41
#define CHANNEL2_DATA 0x42
#define CMD 0x43

void x86_64_pit_set_reload(uint16_t reload_value) {
    x86_64_port_outb(CMD, 0x34);
    x86_64_port_outb(CHANNEL0_DATA, (uint8_t) reload_value);
    x86_64_port_outb(CHANNEL0_DATA, (uint8_t) (reload_value >> 8));
}

void x86_64_pit_match_frequency(uint64_t frequency) {
    uint64_t divider = PIT_BASE_FREQ / frequency;
    if(PIT_BASE_FREQ % frequency > frequency / 2) divider++;
    x86_64_pit_set_reload((uint16_t) divider);
}

uint16_t x86_64_pit_count() {
    x86_64_port_outb(CMD, 0);
    uint16_t low = x86_64_port_inb(CHANNEL0_DATA);
    uint16_t high = x86_64_port_inb(CHANNEL0_DATA);
    return low | (high << 8);
}
