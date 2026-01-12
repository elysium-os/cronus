#pragma once

#include "lib/rb.h"

#include <stdint.h>

#ifdef __ENV_DEBUG

#define X86_64_PROFILER_FRAMES 128

typedef struct {
    void *function;
    void *call_site;
    uint64_t start;
    uint64_t profiler_time;
} x86_64_profiler_frame_t;

typedef struct {
    void *function;
    uint64_t total_time;
    uint64_t calls;
    rb_node_t rb_node;
} x86_64_profiler_record_t;

/// Start profiling.
void x86_64_profiler_start();

/// Stop profiling.
void x86_64_profiler_stop();

/// Reset (and free) records.
void x86_64_profiler_reset();

/// Print results.
void x86_64_profiler_print(const char *name);

/// Create rbtree for profiler records.
rb_tree_t x86_64_profiler_records();

#endif
