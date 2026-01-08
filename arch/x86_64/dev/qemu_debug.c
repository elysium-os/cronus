#include "arch/cpu.h"
#include "common/log.h"
#include "lib/format.h"
#include "sys/init.h"
#include "x86_64/cpu/port.h"

#include <stdarg.h>

#ifdef __ENV_DEVELOPMENT

static void x86_64_qemu_debug_putc(char ch) {
    x86_64_port_outb(0xE9, ch);
}

static void debug_format(char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    format(x86_64_qemu_debug_putc, fmt, list);
    va_end(list);
}

static void log_debug(log_level_t level, const char *tag, const char *fmt, va_list args) {
    char *color;
    switch(level) {
        case LOG_LEVEL_TRACE: color = "\e[90m"; break; /* Gray */
        case LOG_LEVEL_DEBUG: color = "\e[36m"; break; /* Cyan */
        case LOG_LEVEL_INFO:  color = "\e[32m"; break; /* Green */
        case LOG_LEVEL_WARN:  color = "\e[33m"; break; /* Yellow */
        case LOG_LEVEL_ERROR: color = "\e[91m"; break; /* Light Red  */
        case LOG_LEVEL_FATAL: color = "\e[31m"; break; /* Red */
        default:              color = "\e[0m"; break;
    }

    debug_format("\e[90m[%lu] %s%10s\e[0m ", ARCH_CPU_CURRENT_READ(sequential_id), color, tag);
    format(x86_64_qemu_debug_putc, fmt, args);
    x86_64_qemu_debug_putc('\n');
}

static log_sink_t g_qemu_debug_sink = {
    .name = "QEMU",
    .filter = { .level = LOG_LEVEL_TRACE, .tags_as_include = false, .tags = nullptr, .tag_count = 0 },
    .log = log_debug
};

INIT_TARGET(qemu_debug, INIT_STAGE_BOOT, INIT_SCOPE_BSP, INIT_DEPS()) {
    x86_64_qemu_debug_putc('\n');
    log_sink_add(&g_qemu_debug_sink);
}

#endif
