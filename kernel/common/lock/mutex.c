#include "mutex.h"

#include "arch/sched.h"
#include "common/assert.h"
#include "lib/container.h"
#include "lib/expect.h"
#include "sched/sched.h"
#include "sched/thread.h"

#define SPIN_COUNT 10

static bool try_lock(mutex_t *mutex, bool weak) {
    mutex_state_t state = MUTEX_STATE_UNLOCKED;
    return __atomic_compare_exchange_n(&mutex->state, &state, MUTEX_STATE_LOCKED, weak, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
}

void mutex_acquire(mutex_t *mutex) {
    if(EXPECT_LIKELY(try_lock(mutex, true))) return;

    for(int i = 0; i < SPIN_COUNT; i++) {
        if(EXPECT_LIKELY(try_lock(mutex, true))) return;
        sched_yield(THREAD_STATE_READY);
    }

    interrupt_state_t previous_state = spinlock_acquire(&mutex->lock);
    if(EXPECT_LIKELY(__atomic_exchange_n(&mutex->state, MUTEX_STATE_CONTESTED, __ATOMIC_ACQ_REL) != MUTEX_STATE_UNLOCKED)) {
        list_push_back(&mutex->wait_queue, &arch_sched_thread_current()->list_wait);

        spinlock_primitive_release(&mutex->lock); // TODO: race condition here ?
        sched_yield(THREAD_STATE_BLOCK);
        spinlock_primitive_acquire(&mutex->lock);
    } else {
        __atomic_store_n(&mutex->state, MUTEX_STATE_LOCKED, __ATOMIC_RELEASE);
    }

    spinlock_release(&mutex->lock, previous_state);
}

void mutex_release(mutex_t *mutex) {
    mutex_state_t state = MUTEX_STATE_LOCKED;
    if(EXPECT_LIKELY(__atomic_compare_exchange_n(&mutex->state, &state, MUTEX_STATE_UNLOCKED, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))) return;

    interrupt_state_t previous_state = spinlock_acquire(&mutex->lock);

    ASSERT(state == MUTEX_STATE_CONTESTED);
    ASSERT(mutex->wait_queue.count != 0);

    thread_t *thread = CONTAINER_OF(list_pop(&mutex->wait_queue), thread_t, list_wait);
    sched_thread_schedule(thread);

    if(mutex->wait_queue.count == 0) __atomic_store_n(&mutex->state, MUTEX_STATE_LOCKED, __ATOMIC_RELEASE);

    spinlock_release(&mutex->lock, previous_state);
}
