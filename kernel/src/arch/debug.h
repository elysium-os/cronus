#pragma once

#include <stddef.h>

extern const char *g_arch_debug_symbols;
extern size_t g_arch_debug_symbols_length;

/**
 * @brief Log a stack trace.
 */
void arch_debug_stack_trace();