#include "lapic.h"

#include "arch/cpu.h"
#include "memory/hhdm.h"

#include "arch/x86_64/cpu/cpu.h"
#include "arch/x86_64/cpu/msr.h"

#define BASE_MASK 0xF'FFFF'FFFF'F000

#define REG_ID 0x20
#define REG_SPURIOUS 0xF0
#define REG_EOI 0xB0
#define REG_IN_SERVICE_BASE 0x100
#define REG_ICR0 0x300
#define REG_ICR1 0x310
#define REG_LVT_TIMER 0x320
#define REG_TIMER_DIV 0x3E0
#define REG_TIMER_INITIAL_COUNT 0x380
#define REG_TIMER_CURRENT_COUNT 0x390

static inline void lapic_write(uint32_t reg, uint32_t data) {
    *(volatile uint32_t *) HHDM((x86_64_msr_read(X86_64_MSR_APIC_BASE) & BASE_MASK) + reg) = data;
}

static inline uint32_t lapic_read(uint32_t reg) {
    return *(volatile uint32_t *) HHDM((x86_64_msr_read(X86_64_MSR_APIC_BASE) & BASE_MASK) + reg);
}

void x86_64_lapic_initialize() {
    lapic_write(REG_SPURIOUS, 0xFF | (1 << 8));
}

void x86_64_lapic_eoi(uint8_t interrupt_vector) {
    if(lapic_read(REG_IN_SERVICE_BASE + interrupt_vector / 32 * 0x10) & (1 << (interrupt_vector % 32))) lapic_write(REG_EOI, 0);
}

void x86_64_lapic_ipi(uint32_t lapic_id, uint32_t vec) {
    lapic_write(REG_ICR1, lapic_id << 24);
    lapic_write(REG_ICR0, vec);
}

uint32_t x86_64_lapic_id() {
    return (uint8_t) (lapic_read(REG_ID) >> 24);
}

void x86_64_lapic_timer_poll(uint32_t ticks) {
    x86_64_lapic_timer_stop();
    lapic_write(REG_LVT_TIMER, (1 << 16) | 0xFF);
    lapic_write(REG_TIMER_DIV, 0);
    lapic_write(REG_TIMER_INITIAL_COUNT, ticks);
    while(lapic_read(REG_TIMER_CURRENT_COUNT) != 0);
    x86_64_lapic_timer_stop();
}

void x86_64_lapic_timer_oneshot(uint8_t vector, uint64_t us) {
    x86_64_lapic_timer_stop();
    lapic_write(REG_LVT_TIMER, vector);
    lapic_write(REG_TIMER_DIV, 0);
    lapic_write(REG_TIMER_INITIAL_COUNT, us * (X86_64_CPU_LOCAL_MEMBER(lapic_timer_frequency) / 1000000));
}

void x86_64_lapic_timer_stop() {
    lapic_write(REG_TIMER_INITIAL_COUNT, 0);
    lapic_write(REG_LVT_TIMER, (1 << 16));
}
