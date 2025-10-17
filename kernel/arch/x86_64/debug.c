#include "arch/debug.h"

#include "sys/kernel_symbol.h"
#include "x86_64/debug.h"

#include <stddef.h>

void arch_debug_stack_trace(log_level_t level, const char *tag) {
    x86_64_debug_stack_frame_t *stack_frame;
    asm volatile("movq %%rbp, %0" : "=r"(stack_frame));
    x86_64_debug_stack_trace_from(level, tag, stack_frame);
}

void x86_64_debug_stack_trace_from(log_level_t level, const char *tag, x86_64_debug_stack_frame_t *stack_frame) {
    log(level, tag, "Stack Trace:");
    for(int i = 0; stack_frame != nullptr && stack_frame->rip != 0 && i < 30; i++) {
        kernel_symbol_t symbol;
        if(!kernel_symbol_lookup_by_address(stack_frame->rip, &symbol)) {
            log(level, tag, "    [UNKNOWN] <%#lx>", stack_frame->rip);
        } else {
            log(level, tag, "    %s+%lu <%#lx>", symbol.name, stack_frame->rip - symbol.address, stack_frame->rip);
        }
        stack_frame = stack_frame->rbp;
    }
}
