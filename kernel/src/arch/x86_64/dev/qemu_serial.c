#include "qemu_serial.h"

#include "lib/format.h"

#include "arch/x86_64/cpu/port.h"

#include <stdarg.h>

static void serial_format(char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    format(x86_64_qemu_serial_putc, fmt, list);
    va_end(list);
}

static void log_serial(log_level_t level, const char *tag, const char *fmt, va_list args) {
    char *color;
    switch(level) {
        case LOG_LEVEL_DEBUG: color = "\e[36m"; break;
        case LOG_LEVEL_INFO:  color = "\e[33m"; break;
        case LOG_LEVEL_WARN:  color = "\e[91m"; break;
        case LOG_LEVEL_ERROR: color = "\e[31m"; break;
        default:              color = "\e[0m"; break;
    }
    serial_format("%s[%s:%s]%s ", color, log_level_stringify(level), tag, "\e[0m");
    format(x86_64_qemu_serial_putc, fmt, args);
    x86_64_qemu_serial_putc('\n');
}

log_sink_t g_x86_64_qemu_serial_sink = {
    .name = "SERIAL",
    .filter = {.level = LOG_LEVEL_DEBUG, .tags_as_include = false, .tags = NULL, .tag_count = 0},
    .log = log_serial
};

void x86_64_qemu_serial_putc(char ch) {
    x86_64_port_outb(0x3F8, ch);
}
