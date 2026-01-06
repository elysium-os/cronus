#include "x86_64/cpu/lapic.h"

#include "common/assert.h"
#include "common/log.h"
#include "memory/mmio.h"
#include "sys/init.h"
#include "x86_64/cpu/msr.h"

#define BASE_ADDR_MASK 0xF'FFFF'FFFF'F000
#define BASE_GLOBAL_ENABLE (1 << 11)

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

static uintptr_t g_common_paddr = 0;
static void *g_common_vaddr = nullptr;

[[clang::always_inline]] static void lapic_write(uint32_t reg, uint32_t data) {
    arch_mmio_write32((void *) ((uintptr_t) g_common_vaddr + reg), data);
}

[[clang::always_inline]] static uint32_t lapic_read(uint32_t reg) {
    return arch_mmio_read32((void *) ((uintptr_t) g_common_vaddr + reg));
}

void x86_64_lapic_init() {
    g_common_paddr = x86_64_msr_read(X86_64_MSR_APIC_BASE) & BASE_ADDR_MASK;
    log(LOG_LEVEL_DEBUG, "LAPIC", "Lapic found at %#lx", g_common_paddr);

    g_common_vaddr = mmio_map(g_common_paddr, 4096);
    ASSERT(g_common_vaddr != nullptr);
    ASSERT(g_common_paddr != 0 && (g_common_paddr & BASE_ADDR_MASK) == g_common_paddr);
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

void x86_64_lapic_timer_setup(x86_64_lapic_timer_type_t type, bool mask_interrupt, uint8_t interrupt_vector, x86_64_lapic_timer_divisor_t divisor) {
    x86_64_lapic_timer_stop();
    lapic_write(REG_LVT_TIMER, interrupt_vector | type | (mask_interrupt ? (1 << 16) : 0));
    lapic_write(REG_TIMER_DIV, divisor);
}

void x86_64_lapic_timer_start(uint64_t ticks) {
    x86_64_lapic_timer_stop();
    lapic_write(REG_TIMER_INITIAL_COUNT, ticks);
}

[[clang::always_inline]] void x86_64_lapic_timer_stop() {
    lapic_write(REG_TIMER_INITIAL_COUNT, 0);
}

uint32_t x86_64_lapic_timer_read() {
    return lapic_read(REG_TIMER_CURRENT_COUNT);
}


INIT_TARGET(lapic, INIT_STAGE_BEFORE_MAIN, INIT_SCOPE_ALL, INIT_DEPS("external_interrupts")) {
    uint64_t lapic_base_msr = x86_64_msr_read(X86_64_MSR_APIC_BASE);
    lapic_base_msr &= ~BASE_ADDR_MASK;
    lapic_base_msr |= g_common_paddr;
    lapic_base_msr |= BASE_GLOBAL_ENABLE;
    x86_64_msr_write(X86_64_MSR_APIC_BASE, lapic_base_msr);

    lapic_write(REG_SPURIOUS, 0xFF | (1 << 8));
}
