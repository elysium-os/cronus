#include "arch/event.h"

#include "lib/math.h"

#include "arch/x86_64/cpu/cpu.h"
#include "arch/x86_64/cpu/lapic.h"

void arch_event_timer_setup(int vector) {
    x86_64_lapic_timer_setup(X86_64_LAPIC_TIMER_TYPE_ONESHOT, false, vector, X86_64_LAPIC_TIMER_DIVISOR_16);
}

void arch_event_timer_arm(time_t delay) {
    x86_64_lapic_timer_start(MATH_CLAMP(X86_64_CPU_CURRENT.lapic_timer_frequency * delay / TIME_NANOSECONDS_IN_SECOND, 1ull, UINT32_MAX));
}
