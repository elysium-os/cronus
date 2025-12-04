#include "arch/time.h"

#include "arch/cpu.h"

time_t arch_time_monotonic() {
    return __rdtsc() / (ARCH_CPU_CURRENT_READ(arch.tsc_timer_frequency) / TIME_NANOSECONDS_IN_SECOND);
}
