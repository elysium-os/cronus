#pragma once

#include "common/log.h"

#include <stdint.h>

#define X86_64_DEBUG_PROF_MAX_FRAMES 128

typedef struct [[gnu::packed]] x86_64_debug_stack_frame {
    struct x86_64_debug_stack_frame *rbp;
    uint64_t rip;
} x86_64_debug_stack_frame_t;

typedef struct {
    void *function;
    void *call_site;
    uint64_t start;
    uint64_t ptime;
} x86_64_debug_prof_call_frame_t;

/// Log a stack trace starting at the provided stack frame.
/// @param level Log level to print stack trace at
/// @param tag Tag to print stack trace under
void x86_64_debug_stack_trace_from(log_level_t level, const char *tag, x86_64_debug_stack_frame_t *stack_frame);

#ifdef __ENV_DEVELOPMENT
/// Start the profiler.
void x86_64_debug_prof_start();

/// Stop the profiler.
void x86_64_debug_prof_stop();

/// Reset the profiler.
void x86_64_debug_prof_reset();

/// Print profile data.
void x86_64_debug_prof_show(const char *name);
#endif
