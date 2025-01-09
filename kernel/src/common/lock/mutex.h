#pragma once

#include "common/lock/wait.h"

#define MUTEX_INIT ((mutex_t) {.waitable = WAITABLE_INIT, .lock = 0})

typedef struct {
    waitable_t waitable;
    bool lock;
} mutex_t;

/**
 * @brief Acquire mutex. First spins some cycles and then blocks.
 */
void mutex_acquire(mutex_t *mutex);

/**
 * @brief Release mutex.
 */
void mutex_release(mutex_t *mutex);
