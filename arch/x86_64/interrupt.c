#include "x86_64/interrupt.h"

#include "arch/cpu.h"
#include "arch/interrupt.h"
#include "common/assert.h"
#include "sys/dw.h"
#include "sys/init.h"
#include "x86_64/cpu/gdt.h"

#define FLAGS_NORMAL 0x8E
#define FLAGS_TRAP 0x8F
#define IDT_SIZE 256

typedef struct [[gnu::packed]] {
    uint16_t low_offset;
    uint16_t segment_selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t middle_offset;
    uint32_t high_offset;
    uint32_t rsv0;
} idt_entry_t;

typedef void (*interrupt_handler_t)(arch_interrupt_frame_t *frame);

static uint64_t g_priority_map[] = { [INTERRUPT_PRIORITY_LOW] = 0x2, [INTERRUPT_PRIORITY_NORMAL] = 0x5, [INTERRUPT_PRIORITY_EVENT] = 0x7, [INTERRUPT_PRIORITY_CRITICAL] = 0xF };

extern uint64_t g_x86_64_isr_stubs[IDT_SIZE];

static idt_entry_t g_idt[IDT_SIZE];
static void (*g_entries[IDT_SIZE])(arch_interrupt_frame_t *frame);

x86_64_interrupt_irq_eoi_t g_x86_64_interrupt_irq_eoi;

static int find_vector(interrupt_priority_t priority) {
    int offset = g_priority_map[priority] << 4;
    for(int i = offset; i < IDT_SIZE && i < offset + 16; i++) {
        if(g_entries[i] != nullptr) continue;
        return i;
    }
    return -1;
}

[[gnu::no_instrument_function]] void x86_64_interrupt_handler(arch_interrupt_frame_t *frame) {
    bool is_threaded = ARCH_CPU_CURRENT_READ(flags.threaded);
    bool is_outmost_handler = false;

    if(is_threaded) {
        is_outmost_handler = !ARCH_CPU_CURRENT_THREAD()->in_interrupt_handler;
        if(is_outmost_handler) ARCH_CPU_CURRENT_THREAD()->in_interrupt_handler = true;

        sched_preempt_inc();
        dw_status_disable();
    }

    // Handle HardINT
    ARCH_CPU_CURRENT_WRITE(flags.in_interrupt_hard, true);
    if(g_entries[frame->int_no] != nullptr) g_entries[frame->int_no](frame);
    if(frame->int_no >= 32) g_x86_64_interrupt_irq_eoi(frame->int_no);
    ARCH_CPU_CURRENT_WRITE(flags.in_interrupt_hard, false);

    if(is_threaded) {
        // At this point the HardINT is handled, so we can enable interrupts
        ARCH_CPU_CURRENT_WRITE(flags.in_interrupt_soft, true);
        arch_interrupt_enable();
        dw_status_enable();
        arch_interrupt_disable();
        ARCH_CPU_CURRENT_WRITE(flags.in_interrupt_soft, false);

        sched_preempt_dec();

        // Ensure preemption and dw is enabled if we return to userspace
        ASSERT(!X86_64_INTERRUPT_IS_FROM_USER(frame) || (ARCH_CPU_CURRENT_READ(sched.status.preempt_counter) == 0 && ARCH_CPU_CURRENT_READ(flags.deferred_work_status) == 0));
        if(is_outmost_handler) ARCH_CPU_CURRENT_THREAD()->in_interrupt_handler = false;
    }
}

void x86_64_interrupt_set_ist(uint8_t vector, uint8_t ist) {
    g_idt[vector].ist = ist;
}

void x86_64_interrupt_load_idt() {
    struct [[gnu::packed]] {
        uint16_t limit;
        uint64_t base;
    } idtr = { .base = (uint64_t) &g_idt, .limit = sizeof(g_idt) - 1 };
    asm volatile("lidt %0" : : "m"(idtr));
}

void x86_64_interrupt_set(uint8_t vector, void (*handler)(arch_interrupt_frame_t *frame)) {
    g_entries[vector] = handler;
}

int x86_64_interrupt_request(interrupt_priority_t priority, void (*handler)(arch_interrupt_frame_t *frame)) {
    int vector = find_vector(priority);
    if(vector >= 0) x86_64_interrupt_set(vector, handler);
    return vector;
}

int arch_interrupt_request(enum interrupt_priority priority, void (*handler)(arch_interrupt_frame_t *frame)) {
    int vector = find_vector(priority);
    if(vector >= 0) g_entries[vector] = handler;
    return vector;
}

static void init_interrupts() {
    // TODO: statically initialize?
    for(unsigned long i = 0; i < sizeof(g_idt) / sizeof(idt_entry_t); i++) {
        g_idt[i].low_offset = (uint16_t) g_x86_64_isr_stubs[i];
        g_idt[i].middle_offset = (uint16_t) (g_x86_64_isr_stubs[i] >> 16);
        g_idt[i].high_offset = (uint32_t) (g_x86_64_isr_stubs[i] >> 32);
        g_idt[i].segment_selector = X86_64_GDT_SELECTOR_CODE64_RING0;
        g_idt[i].flags = FLAGS_NORMAL;
        g_idt[i].ist = 0;
        g_idt[i].rsv0 = 0;

        g_entries[i] = nullptr;
    }
}

INIT_TARGET(interrupts, INIT_STAGE_BOOT, init_interrupts);
