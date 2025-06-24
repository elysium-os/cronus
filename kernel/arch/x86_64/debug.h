#pragma once

#include "common/log.h"

#include <stdint.h>

typedef struct [[gnu::packed]] x86_64_debug_stack_frame {
    struct x86_64_debug_stack_frame *rbp;
    uint64_t rip;
} x86_64_debug_stack_frame_t;

/// Log a stack trace starting at the provided stack frame.
/// @param level Log level to print stack trace at
/// @param tag Tag to print stack trace under
void x86_64_debug_stack_trace_from(log_level_t level, const char *tag, x86_64_debug_stack_frame_t *stack_frame);
