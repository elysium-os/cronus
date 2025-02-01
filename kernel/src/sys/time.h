#pragma once

#include "lib/list.h"

#include <stdint.h>

#define TIME_NANOSECONDS_IN_SECOND 1'000'000'000
#define TIME_MICROSECONDS_IN_SECOND 1'000'000
#define TIME_MILLISECONDS_IN_SECOND 1'000

typedef uint64_t time_t; /* stored in nanoseconds */

typedef struct {
    const char *name;
    time_t resolution;
    time_t (*current)();
} time_source_t;

void time_source_register(time_source_t *source);

time_t time_monotonic();
