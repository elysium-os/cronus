#include "common/lock/spinlock.h"

#include "arch/cpu.h"
#include "common/assert.h"
#include "sys/dw.h"
#include "sys/interrupt.h"

#include <stdint.h>

#define DEADLOCK_AT 100'000'000

void spinlock_acquire(spinlock_t *lock) {
    sched_preempt_inc();
    ASSERT(!ARCH_CPU_CURRENT_READ(flags.in_interrupt_soft) && !ARCH_CPU_CURRENT_READ(flags.in_interrupt_hard));
    spinlock_acquire_raw(lock);
}

void spinlock_release(spinlock_t *lock) {
    spinlock_release_raw(lock);
    ASSERT(!ARCH_CPU_CURRENT_READ(flags.in_interrupt_soft) && !ARCH_CPU_CURRENT_READ(flags.in_interrupt_hard));
    sched_preempt_dec();
}

void spinlock_acquire_nodw(spinlock_t *lock) {
    sched_preempt_inc();
    dw_status_disable();
    ASSERT(!ARCH_CPU_CURRENT_READ(flags.in_interrupt_hard));
    spinlock_acquire_raw(lock);
}

void spinlock_release_nodw(spinlock_t *lock) {
    spinlock_release_raw(lock);
    ASSERT(!ARCH_CPU_CURRENT_READ(flags.in_interrupt_hard));
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
#ifdef __ENV_DEBUG
    uint64_t dead = 0;
#endif
    while(true) {
        if(spinlock_try_acquire(lock)) return;

        while(__atomic_load_n(lock, __ATOMIC_RELAXED)) {
            arch_cpu_relax();
#ifdef __ENV_DEBUG
            ASSERT(dead++ != DEADLOCK_AT);
#endif
        }
    }
}
