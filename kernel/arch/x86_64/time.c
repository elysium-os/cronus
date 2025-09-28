#include "arch/time.h"

#include "x86_64/cpu/cpu.h"

time_t time_monotonic() {
    return __rdtsc() / (X86_64_CPU_CURRENT_READ(tsc_timer_frequency) / TIME_NANOSECONDS_IN_SECOND);
}
