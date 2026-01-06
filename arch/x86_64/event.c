#include "arch/event.h"

#include "arch/cpu.h"
#include "common/log.h"
#include "common/panic.h"
#include "lib/math.h"
#include "sys/event.h"
#include "sys/init.h"
#include "x86_64/cpu/lapic.h"

static int g_event_interrupt_vector = -1;

void arch_event_timer_arm(time_t delay) {
    uint64_t ticks = MATH_CLAMP(ARCH_CPU_CURRENT_READ(arch.lapic_timer_frequency) * delay / TIME_NANOSECONDS_IN_SECOND, 1ull, UINT32_MAX);
    LOG_TRACE("EVENT", "Timer arm %lu (ticks: %lu)", delay, ticks);
    x86_64_lapic_timer_start(ticks);
}


INIT_TARGET(event_timer_vector, INIT_STAGE_BEFORE_DEV, INIT_SCOPE_BSP, INIT_DEPS()) {
    g_event_interrupt_vector = arch_interrupt_request(INTERRUPT_PRIORITY_EVENT, events_process);
    if(g_event_interrupt_vector < 0) panic("EVENT", "Failed to acquire interrupt vector for event handler");
    log(LOG_LEVEL_DEBUG, "EVENT", "Event interrupt vector: %i", g_event_interrupt_vector);
}

INIT_TARGET(event_timer, INIT_STAGE_BEFORE_DEV, INIT_SCOPE_ALL, INIT_DEPS("event_timer_vector", "timers")) {
    log(LOG_LEVEL_DEBUG, "EVENT", "Event timer setup on vector %i, oneshot, divisor 16", g_event_interrupt_vector);
    x86_64_lapic_timer_setup(X86_64_LAPIC_TIMER_TYPE_ONESHOT, false, g_event_interrupt_vector, X86_64_LAPIC_TIMER_DIVISOR_16);
}
