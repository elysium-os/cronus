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

typedef struct [[gnu::packed]] {
    uint16_t limit;
    uint64_t base;
} idt_descriptor_t;

typedef struct {
    bool free;
    x86_64_interrupt_handler_t handler;
    x86_64_interrupt_priority_t priority;
} interrupt_entry_t;

static x86_64_interrupt_priority_t g_ipl_to_interrupt_map[] = {
    [IPL_PREEMPT] = X86_64_INTERRUPT_PRIORITY_PREEMPT - 1,
    [IPL_NORMAL] = X86_64_INTERRUPT_PRIORITY_NORMAL - 1,
    [IPL_IPC] = X86_64_INTERRUPT_PRIORITY_IPC - 1,
    [IPL_CRITICAL] = X86_64_INTERRUPT_PRIORITY_CRITICAL - 1
};

static ipl_t g_interrupt_to_ipl_map[] = {
    [X86_64_INTERRUPT_PRIORITY_PREEMPT - 1] = IPL_PREEMPT,
    [X86_64_INTERRUPT_PRIORITY_NORMAL - 1] = IPL_NORMAL,
    [X86_64_INTERRUPT_PRIORITY_IPC - 1] = IPL_IPC,
    [X86_64_INTERRUPT_PRIORITY_CRITICAL - 1] = IPL_CRITICAL
};

extern uint64_t g_isr_stubs[IDT_SIZE];

static idt_entry_t g_idt[IDT_SIZE];
static interrupt_entry_t g_entries[IDT_SIZE];

x86_64_interrupt_irq_eoi_t g_x86_64_interrupt_irq_eoi;

static void set_idt_gate(uint8_t gate, uintptr_t handler, uint16_t segment, uint8_t flags) {
    g_idt[gate].low_offset = (uint16_t) handler;
    g_idt[gate].middle_offset = (uint16_t) (handler >> 16);
    g_idt[gate].high_offset = (uint32_t) (handler >> 32);
    g_idt[gate].segment_selector = segment;
    g_idt[gate].flags = flags;
    g_idt[gate].ist = 0;
    g_idt[gate].rsv0 = 0;
}

static void interrupt_priority_set(x86_64_interrupt_priority_t priority) {
    x86_64_cr8_write(((uint64_t) priority));
}

static x86_64_interrupt_priority_t interrupt_priority_get() {
    return (x86_64_interrupt_priority_t) x86_64_cr8_read();
}

void arch_interrupt_set_ipl(ipl_t ipl) {
    interrupt_priority_set(g_ipl_to_interrupt_map[ipl]);
}

ipl_t arch_interrupt_get_ipl() {
    return g_interrupt_to_ipl_map[interrupt_priority_get()];
}

void x86_64_interrupt_handler(x86_64_interrupt_frame_t *frame) {
    if(frame->int_no >= 0x20) g_x86_64_interrupt_irq_eoi(frame->int_no);
    if(!g_entries[frame->int_no].free) g_entries[frame->int_no].handler(frame);
}

void x86_64_interrupt_init() {
    for(unsigned long i = 0; i < sizeof(g_idt) / sizeof(idt_entry_t); i++) {
        set_idt_gate(i, g_isr_stubs[i], X86_64_GDT_SELECTOR_CODE64_RING0, FLAGS_NORMAL);
        g_entries[i].free = true;
    }
}

void x86_64_interrupt_load_idt() {
    idt_descriptor_t idtr;
    idtr.limit = sizeof(g_idt) - 1;
    idtr.base = (uint64_t) &g_idt;
    asm volatile("lidt %0" : : "m"(idtr));
}

void x86_64_interrupt_set(uint8_t vector, x86_64_interrupt_priority_t priority, x86_64_interrupt_handler_t handler) {
    g_entries[vector].free = false;
    g_entries[vector].handler = handler;
    g_entries[vector].priority = priority;
}

int x86_64_interrupt_request(x86_64_interrupt_priority_t priority, x86_64_interrupt_handler_t handler) {
    for(int i = priority << 4; i < IDT_SIZE; i++) {
        if(!g_entries[i].free) continue;
        x86_64_interrupt_set(i, priority, handler);
        return i;
    }
    return -1;
}
