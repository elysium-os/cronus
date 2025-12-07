#include "common/lock/spinlock.h"

#include "arch/cpu.h"
#include "common/assert.h"
#include "sched/sched.h"
#include "sys/cpu.h"
#include "sys/dw.h"
#include "sys/interrupt.h"

#include <stdint.h>

#define DEADLOCK_AT 100'000'000

void spinlock_acquire(spinlock_t *lock) {
    sched_preempt_inc();
    ASSERT(!ATOMIC_LOAD(&gc_cpu_flags.in_interrupt_soft, ATOMIC_RELAXED) && !ATOMIC_LOAD(&gc_cpu_flags.in_interrupt_hard, ATOMIC_RELAXED));
    spinlock_acquire_raw(lock);
}

void spinlock_release(spinlock_t *lock) {
    spinlock_release_raw(lock);
    ASSERT(!ATOMIC_LOAD(&gc_cpu_flags.in_interrupt_soft, ATOMIC_RELAXED) && !ATOMIC_LOAD(&gc_cpu_flags.in_interrupt_hard, ATOMIC_RELAXED));
    sched_preempt_dec();
}

void spinlock_acquire_nodw(spinlock_t *lock) {
    sched_preempt_inc();
    dw_status_disable();
    ASSERT(!ATOMIC_LOAD(&gc_cpu_flags.in_interrupt_hard, ATOMIC_RELAXED));
    spinlock_acquire_raw(lock);
}

void spinlock_release_nodw(spinlock_t *lock) {
    spinlock_release_raw(lock);
    ASSERT(!ATOMIC_LOAD(&gc_cpu_flags.in_interrupt_hard, ATOMIC_RELAXED));
    dw_status_enable();
    sched_preempt_dec();
}

interrupt_state_t spinlock_acquire_noint(spinlock_t *lock) {
    interrupt_state_t previous_state = interrupt_state_mask();
    sched_preempt_inc();
    dw_status_disable();
    spinlock_acquire_raw(lock);
    return previous_state;
}

void spinlock_release_noint(spinlock_t *lock, interrupt_state_t interrupt_state) {
    spinlock_release_raw(lock);
    dw_status_enable();
    sched_preempt_dec();
    interrupt_state_restore(interrupt_state);
}

void spinlock_acquire_raw(spinlock_t *lock) {
#ifdef __ENV_DEVELOPMENT
    uint64_t dead = 0;
#endif
    while(true) {
        if(spinlock_try_acquire(lock)) return;

        while(__atomic_load_n(lock, __ATOMIC_RELAXED)) {
            arch_cpu_relax();
#ifdef __ENV_DEVELOPMENT
            ASSERT(dead++ != DEADLOCK_AT);
#endif
        }
    }
}
