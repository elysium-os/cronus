#pragma once

typedef struct debug_stack_frame debug_stack_frame_t;

/**
 * @brief Log a stack trace starting at the provided stack frame.
 */
void arch_debug_stack_trace(debug_stack_frame_t *stack_frame);

/**
 * @brief Get the current stack frame.
 */
debug_stack_frame_t *arch_debug_stack_frame_get();