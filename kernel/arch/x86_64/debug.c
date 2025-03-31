#include "arch/debug.h"

#include "common/log.h"

#include "arch/x86_64/debug.h"

#include <stddef.h>

const char *g_arch_debug_symbols = NULL;
size_t g_arch_debug_symbols_length = 0;

void arch_debug_stack_trace() {
    x86_64_debug_stack_frame_t *stack_frame;
    asm volatile("movq %%rbp, %0" : "=r"(stack_frame));
    x86_64_debug_stack_trace_from(stack_frame);
}

x86_64_debug_symbol_t x86_64_debug_symbol(uintptr_t lookup_address) {
    if(g_arch_debug_symbols == NULL) return (x86_64_debug_symbol_t) { .found = false };

    enum {
        ADDRESS,
        TYPE,
        NAME,
        UNKNOWN
    } state = ADDRESS;
    uintptr_t address = 0, previous_address = 0;
    size_t name_offset = 0, previous_name_offset = 0;
    for(size_t i = 0; i < g_arch_debug_symbols_length; i++) {
        if(g_arch_debug_symbols[i] == '\n') {
            if(address > lookup_address) {
                address = previous_address;
                name_offset = previous_name_offset;
                break;
            }
            if(address == lookup_address) break;

            state = ADDRESS;
            previous_address = address;
            address = 0;
            previous_name_offset = name_offset;
            name_offset = 0;
            continue;
        }

        if(g_arch_debug_symbols[i] == ' ') {
            if(state == UNKNOWN) continue;
            state++;
            continue;
        }

        switch(state) {
            case ADDRESS:
                if(g_arch_debug_symbols[i] >= '0' && g_arch_debug_symbols[i] <= '9') {
                    address *= 16;
                    address += g_arch_debug_symbols[i] - '0';
                    continue;
                }
                if(g_arch_debug_symbols[i] >= 'a' && g_arch_debug_symbols[i] <= 'f') {
                    address *= 16;
                    address += g_arch_debug_symbols[i] - 'a' + 10;
                    continue;
                }
                state = UNKNOWN;
                break;

            case NAME:
                if(name_offset == 0) name_offset = i;
                break;

            case TYPE:    break;
            case UNKNOWN: break;
        }
    }
    if(name_offset == 0 || name_offset >= g_arch_debug_symbols_length) return (x86_64_debug_symbol_t) { .found = false };

    size_t length = 0;
    while(g_arch_debug_symbols[name_offset + length] != '\n' && name_offset + length < g_arch_debug_symbols_length) length++;
    return (x86_64_debug_symbol_t) { .found = true, .name = &g_arch_debug_symbols[name_offset], .length = length, .address = address };
}

void x86_64_debug_stack_trace_from(x86_64_debug_stack_frame_t *stack_frame) {
    if(g_arch_debug_symbols == NULL) {
        log(LOG_LEVEL_DEBUG, "DEBUG", "Stack Trace: missing debug symbols");
        return;
    }

    log(LOG_LEVEL_DEBUG, "DEBUG", "Stack Trace:");
    for(int i = 0; stack_frame != NULL && stack_frame->rip != 0 && i < 30; i++) {
        x86_64_debug_symbol_t symbol = x86_64_debug_symbol(stack_frame->rip);
        if(!symbol.found) {
            log(LOG_LEVEL_DEBUG, "DEBUG", "    [UNKNOWN] <%#lx>", stack_frame->rip);
        } else {
            log(LOG_LEVEL_DEBUG, "DEBUG", "    %.*s+%lu <%#lx>", (int) symbol.length, symbol.name, stack_frame->rip - symbol.address, stack_frame->rip);
        }
        stack_frame = stack_frame->rbp;
    }
}
