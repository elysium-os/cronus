#pragma once

#include <stdint.h>

#define TIME_FEMTOSECONDS_IN_SECOND 1'000'000'000'000'000lu
#define TIME_PICOSECONDS_IN_SECOND 1'000'000'000'000lu
#define TIME_NANOSECONDS_IN_SECOND 1'000'000'000lu
#define TIME_MICROSECONDS_IN_SECOND 1'000'000lu
#define TIME_MILLISECONDS_IN_SECOND 1'000lu

/// Time stored in nanoseconds.
typedef uint64_t time_t;

/// Frequency in hz (ticks per second).
typedef uint64_t time_frequency_t;
