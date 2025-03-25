#include "panic.h"

#include "arch/cpu.h"
#include "arch/debug.h"
#include "common/log.h"

[[noreturn]] void panic(const char *fmt, ...) {
    log(LOG_LEVEL_ERROR, "PANIC", "Kernel Panic");
    va_list list;
    va_start(list, format);
    log_list(LOG_LEVEL_ERROR, "PANIC", fmt, list);
    arch_debug_stack_trace();
    va_end(list);
    arch_cpu_halt();
    __builtin_unreachable();
}
