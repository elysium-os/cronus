#include "arch/debug.h"

#include "common/log.h"

#include <stdint.h>
#include <stddef.h>

char *g_debug_symbols;
size_t g_debug_symbols_length;

struct debug_stack_frame {
    struct debug_stack_frame *rbp;
    uint64_t rip;
} __attribute__((packed));

void arch_debug_stack_trace(debug_stack_frame_t *stack_frame) {
    if(!g_debug_symbols) return;
    log(LOG_LEVEL_ERROR, "DEBUG", "Stack Trace:");
    for(int i = 0; stack_frame && stack_frame->rip && i < 30; i++) {
        uint64_t offset = 0;
        uint64_t address = 0;
        bool skip = false;
        for(uint64_t j = 0; j < g_debug_symbols_length; j++) {
            if(g_debug_symbols[j] == '\n') {
                skip = false;
                address = 0;
                continue;
            }
            if(skip) continue;
            if(g_debug_symbols[j] >= '0' && g_debug_symbols[j] <= '9') {
                address *= 16;
                address += g_debug_symbols[j] - '0';
                continue;
            }
            if(g_debug_symbols[j] >= 'a' && g_debug_symbols[j] <= 'f') {
                address *= 16;
                address += g_debug_symbols[j] - 'a' + 10;
                continue;
            }
            if(g_debug_symbols[j] == ' ' && address >= stack_frame->rip) break;
            skip = true;
            offset = j + 3;
        }

        if(offset >= g_debug_symbols_length) {
            log(LOG_LEVEL_ERROR, "DEBUG", "    [UNKNOWN] <%#lx>", stack_frame->rip);
        } else {
            int len = 0;
            while(g_debug_symbols[offset + len] != '\n') len++;
            log(LOG_LEVEL_ERROR, "DEBUG", "    %.*s+%lu <%#lx>", len, &g_debug_symbols[offset], address - stack_frame->rip, stack_frame->rip);
        }
        stack_frame = stack_frame->rbp;
    }
}

debug_stack_frame_t *arch_debug_stack_frame_get() {
    debug_stack_frame_t *stack_frame;
    asm volatile("movq %%rbp, %0" : "=r" (stack_frame));
    return stack_frame;
}