#pragma once

#include "sys/interrupt.h"

#define SPINLOCK_INIT (0)

typedef bool spinlock_t;

/// Acquire spinlock and mask interrupts.
/// @warning Spins until acquired.
interrupt_state_t spinlock_acquire(volatile spinlock_t *lock);

/// Release spinlock and restore interrupt state.
void spinlock_release(volatile spinlock_t *lock, interrupt_state_t interrupt_state);

/// Acquire spinlock.
/// @warning Spins until acquired.
/// @warning Does not mask interrupts.
void spinlock_primitive_acquire(volatile spinlock_t *lock);

/// Attempt to acquire spinlock.
/// @warning Does not spin, only attempts to acquire the lock once.
/// @warning Does not mask interrupts.
/// @returns true = acquired the lock
static inline bool spinlock_primitive_try_acquire(volatile spinlock_t *lock) {
    return !__atomic_test_and_set(lock, __ATOMIC_ACQUIRE);
}

/// Release spinlock.
/// @warning Does not restore interrupt state.
static inline void spinlock_primitive_release(volatile spinlock_t *lock) {
    __atomic_clear(lock, __ATOMIC_RELEASE);
}
