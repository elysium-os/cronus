#pragma once

#define SPINLOCK_INIT (0)

typedef bool spinlock_t;

/**
 * @brief Acquire spinlock.
 * @warning Spins until acquired.
 */
void spinlock_acquire(volatile spinlock_t *lock);

/**
 * @brief Attempt to acquire spinlock.
 * @warning Does not spin, only attempts to acquire the lock once.
 * @returns true = acquired the lock
 */
static inline bool spinlock_try_acquire(volatile spinlock_t *lock) {
    return !__atomic_test_and_set(lock, __ATOMIC_ACQUIRE);
}

/**
 * @brief Release spinlock.
 */
static inline void spinlock_release(volatile spinlock_t *lock) {
    __atomic_clear(lock, __ATOMIC_RELEASE);
}