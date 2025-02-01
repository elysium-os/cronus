#pragma once

#include "sys/ipl.h"

#define SPINLOCK_INIT (0)

typedef bool spinlock_t;

/**
 * @brief Acquire spinlock and raise IPL.
 * @warning Spins until acquired.
 */
ipl_t spinlock_acquire(volatile spinlock_t *lock);

/**
 * @brief Release spinlock.
 */
void spinlock_release(volatile spinlock_t *lock, ipl_t ipl);


/**
 * @brief Acquire spinlock.
 * @warning Spins until acquired.
 * @warning Does not raise IPL, use the non-primitive version if possible.
 */
void spinlock_primitive_acquire(volatile spinlock_t *lock);

/**
 * @brief Attempt to acquire spinlock.
 * @warning Does not spin, only attempts to acquire the lock once.
 * @warning Does not raise IPL.
 * @returns true = acquired the lock
 */
static inline bool spinlock_primitive_try_acquire(volatile spinlock_t *lock) {
    return !__atomic_test_and_set(lock, __ATOMIC_ACQUIRE);
}

/**
 * @brief Release spinlock.
 * @warning Does not lower IPL, use the non-primitive version if possible.
 */
static inline void spinlock_primitive_release(volatile spinlock_t *lock) {
    __atomic_clear(lock, __ATOMIC_RELEASE);
}
