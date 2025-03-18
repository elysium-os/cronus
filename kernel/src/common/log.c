#include "log.h"

#include "common/lock/spinlock.h"
#include "lib/format.h"
#include "lib/string.h"

static spinlock_t g_lock = SPINLOCK_INIT;
static list_t g_sinks = LIST_INIT;

void log_sink_add(log_sink_t *sink) {
    interrupt_state_t previous_state = spinlock_acquire(&g_lock);
    list_append(&g_sinks, &sink->list_elem);
    spinlock_release(&g_lock, previous_state);
}

void log_sink_remove(log_sink_t *sink) {
    interrupt_state_t previous_state = spinlock_acquire(&g_lock);
    list_delete(&sink->list_elem);
    spinlock_release(&g_lock, previous_state);
}

[[gnu::format(printf, 3, 4)]] void log(log_level_t level, const char *tag, const char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    log_list(level, tag, fmt, list);
    va_end(list);
}

[[gnu::format(printf, 3, 0)]] void log_list(log_level_t level, const char *tag, const char *fmt, va_list list) {
    va_list local_list;
    interrupt_state_t previous_state = spinlock_acquire(&g_lock);
    LIST_FOREACH(&g_sinks, elem) {
        log_sink_t *sink = LIST_CONTAINER_GET(elem, log_sink_t, list_elem);
        if(sink->filter.level < level) continue;
        for(size_t i = 0; i < sink->filter.tag_count; i++)
            if(string_eq(sink->filter.tags[i], tag) != sink->filter.tags_as_include) goto skip;
        va_copy(local_list, list);
        sink->log(level, tag, fmt, local_list);
        va_end(local_list);
    skip:
    }
    spinlock_release(&g_lock, previous_state);
}

const char *log_level_stringify(log_level_t level) {
    switch(level) {
        case LOG_LEVEL_ERROR:       return "ERROR";
        case LOG_LEVEL_WARN:        return "WARN";
        case LOG_LEVEL_INFO:        return "INFO";
        case LOG_LEVEL_DEBUG:       return "DEBUG";
        case LOG_LEVEL_DEBUG_NOISY: return "DEBUG";
    }
    return "UNKNOWN";
}
