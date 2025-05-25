#pragma once

#include "common/log.h"

/// Log a stack trace.
/// @param level log level to print stack trace at
/// @param tag tag to print stack trace under
void arch_debug_stack_trace(log_level_t level, const char *tag);
