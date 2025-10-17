#pragma once

#include "sys/interrupt.h"

#define SPINLOCK_INIT (0)

typedef bool spinlock_t;

/// Acquire spinlock (preemption).
void spinlock_acquire(spinlock_t *lock);

/// Release spinlock (preemption).
void spinlock_release(spinlock_t *lock);

// Acquire spinlock (preemption, deferred work).
void spinlock_acquire_nodw(spinlock_t *lock);

// Release spinlock (preemption, deferred work).
void spinlock_release_nodw(spinlock_t *lock);

/// Acquire spinlock (preemption, deferred work, interrupts).
interrupt_state_t spinlock_acquire_noint(spinlock_t *lock);

/// Release spinlock (preemption, deferred work, interrupts).
void spinlock_release_noint(spinlock_t *lock, interrupt_state_t interrupt_state);

/// Acquire spinlock with no side effects.
void spinlock_acquire_raw(spinlock_t *lock);

/// Attempt to acquire spinlock with no side effects.
/// @warning Does not spin, only attempts to acquire the lock once.
/// @returns true = acquired the lock
static inline bool spinlock_try_acquire(spinlock_t *lock) {
    return !__atomic_test_and_set(lock, __ATOMIC_ACQUIRE);
}

/// Release spinlock with no side effects.
static inline void spinlock_release_raw(spinlock_t *lock) {
    __atomic_clear(lock, __ATOMIC_RELEASE);
}
