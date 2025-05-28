#include "arch/time.h"

#include "arch/x86_64/cpu/cpu.h"

time_t arch_time_monotonic() {
    return __rdtsc() / (X86_64_CPU_CURRENT.tsc_timer_frequency / TIME_NANOSECONDS_IN_SECOND);
}
