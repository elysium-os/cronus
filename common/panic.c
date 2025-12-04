#include "common/panic.h"

#include "arch/cpu.h"
#include "arch/debug.h"
#include "arch/interrupt.h"
#include "common/log.h"

[[noreturn]] void panic(const char *tag, const char *fmt, ...) {
    arch_interrupt_disable();
    log(LOG_LEVEL_FATAL, tag, "Kernel Panic (CPU: %lu)", ARCH_CPU_CURRENT_READ(sequential_id));
    va_list list;
    va_start(list, format);
    log_list(LOG_LEVEL_FATAL, tag, fmt, list);
    arch_debug_stack_trace(LOG_LEVEL_FATAL, tag);
    va_end(list);
    arch_cpu_halt();
    __builtin_unreachable();
}
