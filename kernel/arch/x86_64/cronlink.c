#include "arch/cronlink.h"

#include "arch/x86_64/cpu/port.h"

void arch_cronlink_write(uint8_t byte) {
    x86_64_port_outb(0x3F8, byte);
}
