#include "arch/cpu.h"
#include "common/log.h"
#include "lib/math.h"
#include "sys/init.h"
#include "sys/time.h"
#include "x86_64/cpu/cpuid.h"
#include "x86_64/dev/hpet.h"
#include "x86_64/dev/pit.h"

// TODO: cpuid for TSC freq if available :)
INIT_TARGET_PERCORE(tsc_timer, INIT_PROVIDES("timer"), INIT_DEPS("cpu_local", "hpet", "assert", "log")) {
    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_TSC));
    // ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_TSC_INVARIANT)); // TODO: doesnt work on QEMU TCG

    time_frequency_t frequency = 0;
    if(!g_hpet_initialized) {
        for(size_t sample_count = 8;; sample_count *= 2) {
            x86_64_pit_set_reload(0xFFF0);
            uint16_t start_count = x86_64_pit_count();
            uint64_t tsc_target = __rdtsc() + sample_count;
            while(__rdtsc() < tsc_target) arch_cpu_relax();
            uint64_t delta = start_count - x86_64_pit_count();

            if(delta < 0x4000) continue;

            frequency = (sample_count / MATH_MAX(1lu, delta)) * X86_64_PIT_BASE_FREQ;
            break;
        }
    } else {
        time_t timeout = (TIME_NANOSECONDS_IN_SECOND / TIME_MILLISECONDS_IN_SECOND) * 100;
        uint64_t tsc_start = __rdtsc();
        time_t target = hpet_current_time() + timeout;
        while(hpet_current_time() < target) arch_cpu_relax();
        frequency = (__rdtsc() - tsc_start) * (TIME_NANOSECONDS_IN_SECOND / timeout);
    }

    ARCH_CPU_CURRENT_WRITE(arch.tsc_timer_frequency, frequency);

    log(LOG_LEVEL_DEBUG, "INIT", "CPU[%lu] TSC calibrated, freq: %lu", ARCH_CPU_CURRENT_READ(sequential_id), ARCH_CPU_CURRENT_READ(arch.tsc_timer_frequency));
}
