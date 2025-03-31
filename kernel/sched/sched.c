#include "sched.h"

#include "arch/cpu.h"
#include "arch/interrupt.h"
#include "arch/sched.h"
#include "common/assert.h"
#include "common/lock/spinlock.h"
#include "common/log.h"
#include "common/panic.h"
#include "lib/list.h"
#include "reaper.h"
#include "sched/thread.h"
#include "sys/cpu.h"
#include "sys/interrupt.h"

void sched_thread_schedule(thread_t *thread) {
    thread->state = THREAD_STATE_READY;

    interrupt_state_t previous_state = spinlock_acquire(&thread->scheduler->lock);
    list_prepend(&thread->scheduler->thread_queue, &thread->list_sched);
    spinlock_release(&thread->scheduler->lock, previous_state);
}

thread_t *sched_thread_next(sched_t *sched) {
    interrupt_state_t previous_state = spinlock_acquire(&sched->lock);
    if(list_is_empty(&sched->thread_queue)) {
        spinlock_release(&sched->lock, previous_state);
        return NULL;
    }

    thread_t *thread = LIST_CONTAINER_GET(LIST_NEXT(&sched->thread_queue), thread_t, list_sched);
    list_delete(&thread->list_sched);
    spinlock_release(&sched->lock, previous_state);
    thread->state = THREAD_STATE_ACTIVE; // TODO: move this?
    return thread;
}

void sched_yield(enum thread_state yield_state) {
    ASSERT(yield_state != THREAD_STATE_ACTIVE);

    interrupt_state_t previous_state = interrupt_state_mask();
    thread_t *current = arch_sched_thread_current();

    thread_t *next = sched_thread_next(&arch_cpu_current()->sched);
    if(next == NULL && current != current->scheduler->idle_thread) next = current->scheduler->idle_thread;
    if(next != NULL) {
        ASSERT(current != next);
        current->state = yield_state;
        arch_sched_context_switch(current, next);
    } else {
        ASSERT(current->state == yield_state);
    }

    arch_sched_preempt();

    interrupt_state_restore(previous_state);
}

void internal_sched_thread_drop(thread_t *thread) {
    ASSERT(!arch_interrupt_state());
    ASSERT(thread->scheduler == &arch_cpu_current()->sched)

    if(thread == thread->scheduler->idle_thread) return;

    switch(thread->state) {
        case THREAD_STATE_DESTROY:
            if(thread->proc != NULL) {
                interrupt_state_t previous_state = spinlock_acquire(&thread->proc->lock);
                list_delete(&thread->list_proc);
                if(list_is_empty(&thread->proc->threads)) {
                    reaper_queue_process(thread->proc);
                    interrupt_state_restore(previous_state);
                } else {
                    spinlock_release(&thread->proc->lock, previous_state);
                }
            }
            reaper_queue_thread(thread);
            return;
        case THREAD_STATE_BLOCK:  return;
        case THREAD_STATE_ACTIVE: sched_thread_schedule(thread); return;
        case THREAD_STATE_READY:  sched_thread_schedule(thread); return;
    }
    ASSERT_UNREACHABLE();
}
