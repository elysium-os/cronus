#pragma once

#include "common/spinlock.h"
#include "lib/list.h"

#define WAITABLE_INIT ((waitable_t) {.lock = SPINLOCK_INIT, .threads_waiting = LIST_INIT})

typedef struct {
    list_t threads_waiting;
    spinlock_t lock;
} waitable_t;

/**
 * @brief Block on waitable until it is signaled.
 */
void wait_on(waitable_t *waitable);

/**
 * @brief Signal waitable, wake all threads waiting.
 * @warning Can cause thundering herds.
 */
void wait_signal(waitable_t *waitable);