#include "spinlock.h"

#include "arch/cpu.h"
#include "common/assert.h"

#include <stdint.h>

#define DEADLOCK_AT 100000000

ipl_t spinlock_acquire(volatile spinlock_t *lock) {
    ipl_t ipl = ipl_raise(IPL_NORMAL);
    spinlock_primitive_acquire(lock);
    return ipl;
}

void spinlock_release(volatile spinlock_t *lock, ipl_t previous_ipl) {
    spinlock_primitive_release(lock);
    ipl_lower(previous_ipl);
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
