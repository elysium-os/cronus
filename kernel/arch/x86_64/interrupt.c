#include "x86_64/interrupt.h"

#include "arch/interrupt.h"
#include "common/assert.h"
#include "sys/dw.h"
#include "sys/init.h"
#include "x86_64/cpu/cpu.h"
#include "x86_64/cpu/gdt.h"

#define FLAGS_NORMAL 0x8E
#define FLAGS_TRAP 0x8F
#define IDT_SIZE 256

typedef enum {
    HANDLER_TYPE_NONE,
    HANDLER_TYPE_X86_64,
    HANDLER_TYPE_ARCH,
} handler_type_t;

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
    handler_type_t type;
    union {
        x86_64_interrupt_handler_t x86_64_handler;
        interrupt_handler_t arch_handler;
    };
} interrupt_entry_t;

static uint64_t g_priority_map[] = { [INTERRUPT_PRIORITY_LOW] = 0x2, [INTERRUPT_PRIORITY_NORMAL] = 0x5, [INTERRUPT_PRIORITY_EVENT] = 0x7, [INTERRUPT_PRIORITY_CRITICAL] = 0xF };

extern uint64_t g_x86_64_isr_stubs[IDT_SIZE];

static idt_entry_t g_idt[IDT_SIZE];
static interrupt_entry_t g_entries[IDT_SIZE];

x86_64_interrupt_irq_eoi_t g_x86_64_interrupt_irq_eoi;

static int find_vector(interrupt_priority_t priority) {
    int offset = g_priority_map[priority] << 4;
    for(int i = offset; i < IDT_SIZE && i < offset + 16; i++) {
        if(g_entries[i].type != HANDLER_TYPE_NONE) continue;
        return i;
    }
    return -1;
}

bool interrupt_state() {
    uint64_t rflags;
    asm volatile("pushfq\npopq %0" : "=rm"(rflags));
    return (rflags & (1 << 9)) != 0;
}

void interrupt_enable() {
    asm volatile("sti");
}

void interrupt_disable() {
    asm volatile("cli");
}

[[gnu::no_instrument_function]] void x86_64_interrupt_handler(x86_64_interrupt_frame_t *frame) {
    bool is_threaded = X86_64_CPU_CURRENT.common.flags.threaded;
    bool is_outmost_handler = false;

    if(is_threaded) {
        is_outmost_handler = !X86_64_CPU_CURRENT.current_thread->in_interrupt_handler;
        if(is_outmost_handler) X86_64_CPU_CURRENT.current_thread->in_interrupt_handler = true;

        sched_preempt_inc();
        dw_status_disable();
    }

    X86_64_CPU_CURRENT.common.flags.in_interrupt_hard = true;
    switch(g_entries[frame->int_no].type) {
        case HANDLER_TYPE_X86_64: g_entries[frame->int_no].x86_64_handler(frame); break;
        case HANDLER_TYPE_ARCH:   g_entries[frame->int_no].arch_handler(); break;
        case HANDLER_TYPE_NONE:   break;
    }
    if(frame->int_no >= 32) g_x86_64_interrupt_irq_eoi(frame->int_no);
    X86_64_CPU_CURRENT.common.flags.in_interrupt_hard = false;

    if(is_threaded) {
        // At this point the HardINT is handled, so we can enable interrupts
        X86_64_CPU_CURRENT.common.flags.in_interrupt_soft = true;
        interrupt_enable();
        dw_status_enable();
        interrupt_disable();
        X86_64_CPU_CURRENT.common.flags.in_interrupt_soft = false;

        sched_preempt_dec();

        // Ensure preemption and dw is enabled if we return to userspace
        ASSERT(!X86_64_INTERRUPT_IS_FROM_USER(frame) || (X86_64_CPU_CURRENT.common.sched.status.preempt_counter == 0 && X86_64_CPU_CURRENT.common.flags.deferred_work_status == 0));
        if(is_outmost_handler) X86_64_CPU_CURRENT.current_thread->in_interrupt_handler = false;
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

void x86_64_interrupt_set(uint8_t vector, x86_64_interrupt_handler_t handler) {
    g_entries[vector].type = HANDLER_TYPE_X86_64;
    g_entries[vector].x86_64_handler = handler;
}

int x86_64_interrupt_request(interrupt_priority_t priority, x86_64_interrupt_handler_t handler) {
    int vector = find_vector(priority);
    if(vector >= 0) x86_64_interrupt_set(vector, handler);
    return vector;
}

int interrupt_request(enum interrupt_priority priority, interrupt_handler_t handler) {
    int vector = find_vector(priority);
    if(vector >= 0) {
        g_entries[vector].type = HANDLER_TYPE_ARCH;
        g_entries[vector].arch_handler = handler;
    }
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

        g_entries[i].type = HANDLER_TYPE_NONE;
    }
}

INIT_TARGET(interrupts, INIT_STAGE_BOOT, init_interrupts);
