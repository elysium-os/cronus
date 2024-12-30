#include "pic8259.h"

#include "arch/x86_64/sys/port.h"

#define MASTER_CMD 0x20
#define MASTER_DATA 0x21
#define SLAVE_CMD 0xA0
#define SLAVE_DATA 0xA1

void x86_64_pic8259_remap() {
    x86_64_port_outb(MASTER_CMD, 0x11);
    x86_64_port_outb(SLAVE_CMD, 0x11);
    x86_64_port_outb(MASTER_DATA, 0x20);
    x86_64_port_outb(SLAVE_DATA, 0x28);
    x86_64_port_outb(MASTER_DATA, 0x04);
    x86_64_port_outb(SLAVE_DATA, 0x02);
    x86_64_port_outb(MASTER_DATA, 0x01);
    x86_64_port_outb(SLAVE_DATA, 0x01);
    x86_64_port_outb(MASTER_DATA, 0x0);
    x86_64_port_outb(SLAVE_DATA, 0x0);
}

void x86_64_pic8259_disable() {
    x86_64_port_outb(MASTER_DATA, 0xFF);
    x86_64_port_outb(SLAVE_DATA, 0xFF);
}

void x86_64_pic8259_eoi(uint8_t interrupt_vector) {
    if(interrupt_vector >= 40) x86_64_port_outb(SLAVE_CMD, 0x20);
    x86_64_port_outb(MASTER_CMD, 0x20);
}
