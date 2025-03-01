#include "spinlock.h"

#include "arch/cpu.h"
#include "common/assert.h"
#include "sys/interrupt.h"

#include <stdint.h>

#define DEADLOCK_AT 100000000

interrupt_state_t spinlock_acquire(volatile spinlock_t *lock) {
    interrupt_state_t previous_state = interrupt_state_mask();
    spinlock_primitive_acquire(lock);
    return previous_state;
}

void spinlock_release(volatile spinlock_t *lock, interrupt_state_t interrupt_state) {
    spinlock_primitive_release(lock);
    interrupt_state_restore(interrupt_state);
}

void spinlock_primitive_acquire(volatile spinlock_t *lock) {
    uint64_t dead = 0;
    for(;;) {
        if(spinlock_primitive_try_acquire(lock)) return;

        while(__atomic_load_n(lock, __ATOMIC_RELAXED)) {
            arch_cpu_relax();
            ASSERT(dead++ != DEADLOCK_AT);
        }
    }
}
