#include "exception.h"

#include "arch/cpu.h"
#include "common/log.h"

#include "arch/x86_64/cpu/cr.h"
#include "arch/x86_64/debug.h"

static char *g_exception_messages[] = {
    "Division by Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

[[noreturn]] void x86_64_exception_unhandled(x86_64_interrupt_frame_t *frame) {
    log(LOG_LEVEL_ERROR, "EXCEPTION", "Unhandled Exception (%s)", g_exception_messages[frame->int_no]);

    x86_64_debug_stack_frame_t initial_stack_frame;
    initial_stack_frame.rbp = (x86_64_debug_stack_frame_t *) frame->rbp;
    initial_stack_frame.rip = frame->rip;
    x86_64_debug_stack_trace_from(&initial_stack_frame);

    if(frame->int_no == 14) log(LOG_LEVEL_DEBUG, "EXCEPTION", "CR2: %#lx", x86_64_cr2_read());

    log(LOG_LEVEL_DEBUG,
        "EXCEPTION",
        "Interrupt Frame:\nint_no: %#lx\nerror_code: %#lx\nrip: %#lx\ncs: %#lx\nrflags: %#lx\nrsp: %#lx\nss: %#lx",
        frame->int_no,
        frame->err_code,
        frame->rip,
        frame->cs,
        frame->rflags,
        frame->rsp,
        frame->ss);
    log(LOG_LEVEL_DEBUG,
        "EXCEPTION",
        "General Purpose Registers:\nr15: %#lx\nr14: %#lx\nr13: %#lx\nr12: %#lx\nr11: %#lx\nr10: %#lx\nr9: %#lx\nr8: %#lx\n" "rdi: %#lx\nrsi: %#lx\nrbp: %#lx\nrdx: %#lx\nrcx: %#lx\nrbx: %#lx\nrax: %#lx",
        frame->r15,
        frame->r14,
        frame->r13,
        frame->r12,
        frame->r11,
        frame->r10,
        frame->r9,
        frame->r8,
        frame->rdi,
        frame->rsi,
        frame->rbp,
        frame->rdx,
        frame->rcx,
        frame->rbx,
        frame->rax);
    arch_cpu_halt();
    __builtin_unreachable();
}
