#pragma once

#include "lib/list.h"

#include <stdarg.h>
#include <stddef.h>

#ifdef __ENV_DEVELOPMENT
#define LOG_DEVELOPMENT(TAG, FMT, ...) log(LOG_LEVEL_DEVONLY, TAG, FMT, __VA_ARGS__)
#else
#define LOG_DEVELOPMENT(TAG, FMT, ...)
#endif

typedef enum {
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_DEVONLY
} log_level_t;

typedef struct {
    const char *name;
    struct {
        log_level_t level;
        bool tags_as_include;
        const char **tags;
        size_t tag_count;
    } filter;
    list_element_t list_elem;

    void (*log)(log_level_t level, const char *tag, const char *fmt, va_list args);
} log_sink_t;

/**
 * @brief Add log sink.
 */
void log_sink_add(log_sink_t *sink);

/**
 * @brief Remove log sink.
 */
void log_sink_remove(log_sink_t *sink);

/**
 * @brief Emit log.
 */
[[gnu::format(printf, 3, 4)]] void log(log_level_t level, const char *tag, const char *fmt, ...);

/**
 * @brief Emit log using `va_list`.
 */
[[gnu::format(printf, 3, 0)]] void log_list(log_level_t level, const char *tag, const char *fmt, va_list list);

/**
 * @brief Convert log level to string.
 */
const char *log_level_stringify(log_level_t level);
