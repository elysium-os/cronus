#pragma once

#include <stdint.h>

#define TIME_NANOSECONDS_IN_SECOND 1'000'000'000

typedef struct {
    uint64_t seconds;
    uint32_t nanoseconds;
} time_t;

extern time_t g_time_monotonic; /* read-only */

/**
 * @brief Add time a and time b.
 */
static inline time_t time_add(time_t a, time_t b) {
    a.seconds += b.seconds;
    a.nanoseconds += b.nanoseconds;

    if(a.nanoseconds >= TIME_NANOSECONDS_IN_SECOND) {
        a.nanoseconds -= TIME_NANOSECONDS_IN_SECOND;
        a.seconds++;
    }

    return a;
}

/**
 * @brief Subtract time b from time a.
 */
static inline time_t time_subtract(time_t a, time_t b) {
    if(a.seconds < b.seconds) {
        a.seconds = a.nanoseconds = 0;
        return a;
    }
    a.seconds -= b.seconds;

    if(a.nanoseconds < b.nanoseconds) {
        if(a.seconds == 0) {
            a.seconds = a.nanoseconds = 0;
            return a;
        }
        a.nanoseconds += TIME_NANOSECONDS_IN_SECOND - b.nanoseconds;
        a.seconds--;
        return a;
    }
    a.nanoseconds -= b.nanoseconds;

    return a;
}

/**
 * @brief Advance time by some length.
 */
void time_advance(time_t length);