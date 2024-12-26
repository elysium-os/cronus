#include "arch/debug.h"

#include "common/log.h"
#include "arch/x86_64/debug.h"

#include <stddef.h>

const char *g_arch_debug_symbols = NULL;
size_t g_arch_debug_symbols_length = 0;

void arch_debug_stack_trace() {
    x86_64_debug_stack_frame_t *stack_frame;
    asm volatile("movq %%rbp, %0" : "=r" (stack_frame));
    x86_64_debug_stack_trace_from(stack_frame);
}

void x86_64_debug_stack_trace_from(x86_64_debug_stack_frame_t *stack_frame) {
    if(g_arch_debug_symbols == NULL) {
        log(LOG_LEVEL_DEBUG, "DEBUG", "Stack Trace: missing debug symbols");
        return;
    }

    log(LOG_LEVEL_DEBUG, "DEBUG", "Stack Trace:");
    for(int i = 0; stack_frame != NULL && stack_frame->rip != 0 && i < 30; i++) {
        uint64_t offset = 0;
        uint64_t address = 0;
        bool skip = false;
        for(uint64_t j = 0; j < g_arch_debug_symbols_length; j++) {
            if(g_arch_debug_symbols[j] == '\n') {
                skip = false;
                address = 0;
                continue;
            }
            if(skip) continue;
            if(g_arch_debug_symbols[j] >= '0' && g_arch_debug_symbols[j] <= '9') {
                address *= 16;
                address += g_arch_debug_symbols[j] - '0';
                continue;
            }
            if(g_arch_debug_symbols[j] >= 'a' && g_arch_debug_symbols[j] <= 'f') {
                address *= 16;
                address += g_arch_debug_symbols[j] - 'a' + 10;
                continue;
            }
            if(g_arch_debug_symbols[j] == ' ' && address >= stack_frame->rip) break;
            skip = true;
            offset = j + 3;
        }

        if(offset >= g_arch_debug_symbols_length) {
            log(LOG_LEVEL_DEBUG, "DEBUG", "    [UNKNOWN] <%#lx>", stack_frame->rip);
        } else {
            int len = 0;
            while(g_arch_debug_symbols[offset + len] != '\n') len++;
            log(LOG_LEVEL_DEBUG, "DEBUG", "    %.*s+%lu <%#lx>", len, &g_arch_debug_symbols[offset], address - stack_frame->rip, stack_frame->rip);
        }
        stack_frame = stack_frame->rbp;
    }
}