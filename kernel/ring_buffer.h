#pragma once

#include "common/log.h"

typedef struct {
    char *buffer;
    size_t size, index;
    bool full;
} ring_buffer_t;

extern ring_buffer_t g_ring_buffer;
extern log_sink_t g_ring_buffer_sink;
