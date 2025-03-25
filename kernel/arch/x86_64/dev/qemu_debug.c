#include "qemu_debug.h"

#include "lib/format.h"

#include "arch/x86_64/cpu/port.h"

#include <stdarg.h>

static void debug_format(char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    format(x86_64_qemu_debug_putc, fmt, list);
    va_end(list);
}

static void log_debug(log_level_t level, const char *tag, const char *fmt, va_list args) {
    char *color;
    switch(level) {
        case LOG_LEVEL_DEBUG: color = "\e[36m"; break;
        case LOG_LEVEL_INFO:  color = "\e[33m"; break;
        case LOG_LEVEL_WARN:  color = "\e[91m"; break;
        case LOG_LEVEL_ERROR: color = "\e[31m"; break;
        default:              color = "\e[0m"; break;
    }
    debug_format("%s[%s:%s]%s ", color, log_level_stringify(level), tag, "\e[0m");
    format(x86_64_qemu_debug_putc, fmt, args);
    x86_64_qemu_debug_putc('\n');
}

log_sink_t g_x86_64_qemu_debug_sink = {
    .name = "QEMU",
    .filter = {.level = LOG_LEVEL_DEBUG, .tags_as_include = false, .tags = NULL, .tag_count = 0},
    .log = log_debug
};

void x86_64_qemu_debug_putc(char ch) {
    x86_64_port_outb(0xE9, ch);
}
