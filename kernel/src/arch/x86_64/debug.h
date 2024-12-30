#pragma once

#include <stdint.h>

typedef struct x86_64_debug_stack_frame x86_64_debug_stack_frame_t;

struct x86_64_debug_stack_frame {
    x86_64_debug_stack_frame_t *rbp;
    uint64_t rip;
} __attribute__((packed));

/**
 * @brief Log a stack trace starting at the provided stack frame.
 */
void x86_64_debug_stack_trace_from(x86_64_debug_stack_frame_t *stack_frame);
