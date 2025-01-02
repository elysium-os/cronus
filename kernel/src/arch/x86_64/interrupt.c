#include "interrupt.h"

#include "arch/interrupt.h"

#include "arch/x86_64/cpu/cr.h"
#include "arch/x86_64/cpu/gdt.h"

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

typedef struct {
    bool free;
    x86_64_interrupt_handler_t handler;
} interrupt_entry_t;

static x86_64_interrupt_priority_t g_ipl_to_interrupt_map[] =
    {[IPL_PREEMPT] = X86_64_INTERRUPT_PRIORITY_PREEMPT, [IPL_NORMAL] = X86_64_INTERRUPT_PRIORITY_NORMAL, [IPL_CRITICAL] = X86_64_INTERRUPT_PRIORITY_CRITICAL};

static ipl_t g_interrupt_to_ipl_map[] =
    {[X86_64_INTERRUPT_PRIORITY_PREEMPT] = IPL_PREEMPT, [X86_64_INTERRUPT_PRIORITY_NORMAL] = IPL_NORMAL, [X86_64_INTERRUPT_PRIORITY_CRITICAL] = IPL_CRITICAL};

extern uint64_t g_isr_stubs[IDT_SIZE];

static idt_entry_t g_idt[IDT_SIZE];
static interrupt_entry_t g_entries[IDT_SIZE];

x86_64_interrupt_irq_eoi_t g_x86_64_interrupt_irq_eoi;

void arch_interrupt_set_ipl(ipl_t ipl) {
    x86_64_interrupt_priority_t priority = g_ipl_to_interrupt_map[ipl];
    x86_64_cr8_write(((uint64_t) priority) - 1);
}

ipl_t arch_interrupt_get_ipl() {
    x86_64_interrupt_priority_t priority = (x86_64_interrupt_priority_t) (x86_64_cr8_read() + 1);
    return g_interrupt_to_ipl_map[priority];
}

void x86_64_interrupt_handler(x86_64_interrupt_frame_t *frame) {
    ipl_t ipl = ipl_raise(IPL_NORMAL);
    asm volatile("sti");

    if(!g_entries[frame->int_no].free) g_entries[frame->int_no].handler(frame);
    if(frame->int_no >= 32) g_x86_64_interrupt_irq_eoi(frame->int_no);

    asm volatile("cli");
    ipl_lower(ipl);
}

void x86_64_interrupt_init() {
    for(unsigned long i = 0; i < sizeof(g_idt) / sizeof(idt_entry_t); i++) {
        g_idt[i].low_offset = (uint16_t) g_isr_stubs[i];
        g_idt[i].middle_offset = (uint16_t) (g_isr_stubs[i] >> 16);
        g_idt[i].high_offset = (uint32_t) (g_isr_stubs[i] >> 32);
        g_idt[i].segment_selector = X86_64_GDT_SELECTOR_CODE64_RING0;
        g_idt[i].flags = FLAGS_NORMAL;
        g_idt[i].ist = 0;
        g_idt[i].rsv0 = 0;

        g_entries[i].free = true;
    }
}

void x86_64_interrupt_load_idt() {
    struct [[gnu::packed]] {
        uint16_t limit;
        uint64_t base;
    } idtr = {.base = (uint64_t) &g_idt, .limit = sizeof(g_idt) - 1};
    asm volatile("lidt %0" : : "m"(idtr));
}

void x86_64_interrupt_set(uint8_t vector, x86_64_interrupt_handler_t handler) {
    g_entries[vector].free = false;
    g_entries[vector].handler = handler;
}

int x86_64_interrupt_request(x86_64_interrupt_priority_t priority, x86_64_interrupt_handler_t handler) {
    for(int i = (int) priority << 4; i < IDT_SIZE && i < ((int) priority << 4) + 16; i++) {
        if(!g_entries[i].free) continue;
        x86_64_interrupt_set(i, handler);
        return i;
    }
    return -1;
}
