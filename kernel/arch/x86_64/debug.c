#include "arch/debug.h"

#include "common/log.h"
#include "sys/kernel_symbol.h"

#include "arch/x86_64/debug.h"

#include <stddef.h>

void arch_debug_stack_trace() {
    x86_64_debug_stack_frame_t *stack_frame;
    asm volatile("movq %%rbp, %0" : "=r"(stack_frame));
    x86_64_debug_stack_trace_from(stack_frame);
}

void x86_64_debug_stack_trace_from(x86_64_debug_stack_frame_t *stack_frame) {
    log(LOG_LEVEL_DEBUG, "DEBUG", "Stack Trace:");
    for(int i = 0; stack_frame != nullptr && stack_frame->rip != 0 && i < 30; i++) {
        kernel_symbol_t symbol;
        if(!kernel_symbol_lookup(stack_frame->rip, &symbol)) {
            log(LOG_LEVEL_DEBUG, "DEBUG", "    [UNKNOWN] <%#lx>", stack_frame->rip);
        } else {
            log(LOG_LEVEL_DEBUG, "DEBUG", "    %s+%lu <%#lx>", symbol.name, stack_frame->rip - symbol.address, stack_frame->rip);
        }
        stack_frame = stack_frame->rbp;
    }
}
