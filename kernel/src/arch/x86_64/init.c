#include "lib/string.h"
#include "common/assert.h"
#include "common/log.h"
#include "memory/hhdm.h"
#include "arch/cpu.h"
#include "arch/debug.h"
#include "arch/x86_64/sys/port.h"
#include "arch/x86_64/sys/cpuid.h"
#include "arch/x86_64/sys/gdt.h"

#include <tartarus.h>
#include <stddef.h>

uintptr_t g_hhdm_offset;
size_t g_hhdm_size;

static void serial_raw(char c) {
    x86_64_port_outb(0x3F8, c);
}

static void serial_format(char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    format(serial_raw, fmt, list);
    va_end(list);
}

static void log_serial(log_level_t level, const char *tag, const char *fmt, va_list args) {
    char *color;
    switch(level) {
        case LOG_LEVEL_DEBUG: color = "\e[36m"; break;
        case LOG_LEVEL_INFO: color = "\e[33m"; break;
        case LOG_LEVEL_WARN: color = "\e[91m"; break;
        case LOG_LEVEL_ERROR: color = "\e[31m"; break;
        default: color = "\e[0m"; break;
    }
    serial_format("%s[%s:%s]%s ", color, log_level_stringify(level), tag, "\e[0m");
    format(serial_raw, fmt, args);
    serial_raw('\n');
}

static log_sink_t g_serial_sink = {
    .name = "SERIAL",
    .filter = {
        .level = LOG_LEVEL_DEBUG,
        .tags_as_include = false,
        .tags = NULL,
        .tag_count = 0
    },
    .log = log_serial
};

[[noreturn]] void init([[maybe_unused]] tartarus_boot_info_t *boot_info) {
    g_hhdm_offset = boot_info->hhdm.offset;
    g_hhdm_size = boot_info->hhdm.size;

    serial_raw('\n');
    log_sink_add(&g_serial_sink);
    log(LOG_LEVEL_INFO, "INIT", "Elysium alpha.6 (" __DATE__ " " __TIME__ ")");

    for(uint16_t i = 0; i < boot_info->module_count; i++) {
        tartarus_module_t *module = &boot_info->modules[i];
        log(LOG_LEVEL_DEBUG, "INIT", "Module found: %s", module->name);
        if(!string_eq("kernelsymbols.txt", module->name)) continue;
        g_arch_debug_symbols = (char *) HHDM(module->paddr);
        g_arch_debug_symbols_length = module->size;
    }

    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_MSR));

    x86_64_gdt_load();

    log(LOG_LEVEL_INFO, "INIT", "Reached end of init");
    arch_cpu_halt();
    __builtin_unreachable();
}

