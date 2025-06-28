#pragma once

#include "lib/list.h"

#include <stdarg.h>
#include <stddef.h>

#ifdef TRACE
#define LOG_TRACE(TAG, FMT, ...) log(LOG_LEVEL_TRACE, TAG, FMT, __VA_ARGS__)
#else
#define LOG_TRACE(TAG, FMT, ...)
#endif

typedef enum {
    LOG_LEVEL_FATAL,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE
} log_level_t;

typedef struct {
    const char *name;
    struct {
        log_level_t level;
        bool tags_as_include;
        const char **tags;
        size_t tag_count;
    } filter;
    list_node_t list_node;

    void (*log)(log_level_t level, const char *tag, const char *fmt, va_list args);
} log_sink_t;

/// Add log sink.
void log_sink_add(log_sink_t *sink);

/// Remove log sink.
void log_sink_remove(log_sink_t *sink);

/// Emit log.
[[gnu::format(printf, 3, 4)]] void log(log_level_t level, const char *tag, const char *fmt, ...);

/// Emit log using `va_list`.
[[gnu::format(printf, 3, 0)]] void log_list(log_level_t level, const char *tag, const char *fmt, va_list list);

/// Convert log level to string.
const char *log_level_stringify(log_level_t level);
