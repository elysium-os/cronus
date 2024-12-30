#include "spinlock.h"

#include "arch/cpu.h"
#include "common/assert.h"

#include <stdint.h>

#define DEADLOCK_AT 100000000

void spinlock_acquire(volatile spinlock_t *lock) {
    uint64_t dead = 0;
    for(;;) {
        if(spinlock_try_acquire(lock)) return;

        while(__atomic_load_n(lock, __ATOMIC_RELAXED)) {
            arch_cpu_relax();
            ASSERT(dead++ != DEADLOCK_AT);
        }
    }
}