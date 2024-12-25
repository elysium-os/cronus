#pragma once

#include <stddef.h>

typedef struct debug_stack_frame debug_stack_frame_t;

extern const char *g_arch_debug_symbols;
extern size_t g_arch_debug_symbols_length;

/**
 * @brief Log a stack trace.
 */
void arch_debug_stack_trace();

/**
 * @brief Log a stack trace starting at the provided stack frame.
 */
void arch_debug_stack_trace_from(debug_stack_frame_t *stack_frame);