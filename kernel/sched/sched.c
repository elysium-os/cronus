#include "sched.h"

#include "arch/cpu.h"
#include "arch/interrupt.h"
#include "arch/sched.h"
#include "common/assert.h"
#include "common/lock/spinlock.h"
#include "lib/container.h"
#include "lib/list.h"
#include "reaper.h"
#include "sched/thread.h"
#include "sys/cpu.h"
#include "sys/interrupt.h"

void sched_thread_schedule(thread_t *thread) {
    thread->state = THREAD_STATE_READY;

    interrupt_state_t previous_state = spinlock_acquire(&thread->scheduler->lock);
    list_push_back(&thread->scheduler->thread_queue, &thread->list_sched);
    spinlock_release(&thread->scheduler->lock, previous_state);
}

thread_t *sched_thread_next(sched_t *sched) {
    interrupt_state_t previous_state = spinlock_acquire(&sched->lock);
    if(sched->thread_queue.count == 0) {
        spinlock_release(&sched->lock, previous_state);
        return nullptr;
    }

    thread_t *thread = CONTAINER_OF(list_pop(&sched->thread_queue), thread_t, list_sched);

    spinlock_release(&sched->lock, previous_state);
    thread->state = THREAD_STATE_ACTIVE; // TODO: move this?
    return thread;
}

void sched_yield(enum thread_state yield_state) {
    interrupt_state_t previous_state = interrupt_state_mask();

    ASSERT(yield_state != THREAD_STATE_ACTIVE);
    ASSERT(!arch_cpu_current()->flags.in_interrupt_hard && !arch_cpu_current()->flags.in_interrupt_soft);

    thread_t *current = arch_sched_thread_current();

    thread_t *next = sched_thread_next(&arch_cpu_current()->sched);
    if(next == nullptr && current != current->scheduler->idle_thread) next = current->scheduler->idle_thread;
    if(next != nullptr) {
        ASSERT(current != next);
        current->state = yield_state;
        arch_sched_context_switch(current, next);
    } else {
        ASSERT(current->state == yield_state);
    }

    arch_sched_preempt();

    interrupt_state_restore(previous_state);
}

sched_preempt_state_t sched_preempt_disable() {
    return sched_preempt_set(SCHED_PREEMPT_STATE_DISABLED);
}

void sched_preempt_restore(sched_preempt_state_t state) {
    sched_preempt_set(state);
}

sched_preempt_state_t sched_preempt_set(sched_preempt_state_t state) {
    interrupt_state_t previous_state = interrupt_state_mask();

    cpu_t *current_cpu = arch_cpu_current();
    sched_preempt_state_t previous_preempt_state = current_cpu->sched.status.preempt;
    current_cpu->sched.status.preempt = state;

    bool yield_now = current_cpu->sched.status.yield_immediately && state == SCHED_PREEMPT_STATE_ENABLED;
    current_cpu->sched.status.yield_immediately = false;

    interrupt_state_restore(previous_state);
    if(yield_now) sched_yield(THREAD_STATE_READY);

    return previous_preempt_state;
}

void internal_sched_thread_drop(thread_t *thread) {
    ASSERT(!arch_interrupt_state());
    ASSERT(thread->scheduler == &arch_cpu_current()->sched);

    if(thread == thread->scheduler->idle_thread) return;

    switch(thread->state) {
        case THREAD_STATE_DESTROY:
            if(thread->proc != nullptr) {
                interrupt_state_t previous_state = spinlock_acquire(&thread->proc->lock);
                list_node_delete(&thread->proc->threads, &thread->list_proc);
                if(thread->proc->threads.count == 0) {
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
