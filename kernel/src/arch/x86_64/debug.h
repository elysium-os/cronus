#pragma once

#include <stdint.h>

#define X86_64_DEBUG_PROF_MAX_FRAMES 128

typedef struct {
    bool found;
    const char *name;
    size_t length;
    uintptr_t address;
} x86_64_debug_symbol_t;

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

/**
 * @brief Lookup a debug symbol by address.
 */
x86_64_debug_symbol_t x86_64_debug_symbol(uintptr_t lookup_address);

/**
 * @brief Log a stack trace starting at the provided stack frame.
 */
void x86_64_debug_stack_trace_from(x86_64_debug_stack_frame_t *stack_frame);

#ifdef __ENV_DEVELOPMENT
/**
 * @brief Start the profiler.
 */
void x86_64_debug_prof_start();

/**
 * @brief Stop the profiler.
 */
void x86_64_debug_prof_stop();

/**
 * @brief Reset the profiler.
 */
void x86_64_debug_prof_reset();

/**
 * @brief Print profile data.
 */
void x86_64_debug_prof_show(const char *name);
#endif
