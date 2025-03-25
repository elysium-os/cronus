#include "ring_buffer.h"

#include "lib/format.h"

#include <stdarg.h>
#include <stddef.h>

#define BUFFER_SIZE 65535

static char g_buffer[BUFFER_SIZE];

ring_buffer_t g_ring_buffer = {.buffer = g_buffer, .size = BUFFER_SIZE, .index = 0, .full = false};

static void write_char(char ch) {
    g_buffer[g_ring_buffer.index] = ch;
    g_ring_buffer.index++;

    if(g_ring_buffer.index >= BUFFER_SIZE) {
        g_ring_buffer.index = 0;
        g_ring_buffer.full = true;
    }
}

static void write_format(char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    format(write_char, fmt, list);
    va_end(list);
}

static void log_ring_buffer(log_level_t level, const char *tag, const char *fmt, va_list args) {
    write_format("%u %s ", level, tag);
    format(write_char, fmt, args);
}

log_sink_t g_ring_buffer_sink = {
    .name = "RING_BUFFER",
    .filter = {.level = LOG_LEVEL_DEBUG, .tags_as_include = false, .tags = NULL, .tag_count = 0},
    .log = log_ring_buffer
};
