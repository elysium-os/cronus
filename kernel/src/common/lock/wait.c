#include "wait.h"

#include "arch/interrupt.h"
#include "arch/sched.h"
#include "common/assert.h"
#include "sched/sched.h"
#include "sched/thread.h"

void wait_on(waitable_t *waitable) {
    ASSERT(arch_interrupt_get_ipl() == IPL_PREEMPT);

    ipl_t previous_ipl = ipl_raise(IPL_NORMAL);
    ipl_t lock_ipl = spinlock_acquire(&waitable->lock);

    thread_t *thread = arch_sched_thread_current();
    thread->state = THREAD_STATE_BLOCK;
    list_append(&waitable->threads_waiting, &thread->list_wait);

    spinlock_release(&waitable->lock, lock_ipl);
    arch_sched_yield();
    ipl_lower(previous_ipl);
}

void wait_signal(waitable_t *waitable) {
    ASSERT(arch_interrupt_get_ipl() == IPL_PREEMPT);

    ipl_t lock_ipl = spinlock_acquire(&waitable->lock);
    LIST_FOREACH(&waitable->threads_waiting, elem) {
        thread_t *thread = LIST_CONTAINER_GET(elem, thread_t, list_wait);
        list_delete(elem);
        sched_thread_schedule(thread);
    }
    spinlock_release(&waitable->lock, lock_ipl);
}
