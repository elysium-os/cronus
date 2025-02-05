// #pragma once

// #include "common/lock/wait.h"
// #include "sys/time.h"

// #include <stddef.h>

// #define SEMA_INIT ((sema_t) {.waitable = WAITABLE_INIT, .lock = SPINLOCK_INIT, .counter = 0})

// typedef struct {
//     waitable_t waitable;
//     spinlock_t lock;
//     size_t counter;
// } sema_t;

// bool sema_try_acquire(sema_t *semaphore);
