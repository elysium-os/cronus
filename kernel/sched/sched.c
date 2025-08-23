#include "sched/sched.h"

#include "arch/cpu.h"
#include "arch/interrupt.h"
#include "arch/sched.h"
#include "common/assert.h"
#include "common/lock/spinlock.h"
#include "lib/container.h"
#include "lib/list.h"
#include "sched/reaper.h"
#include "sched/thread.h"
#include "sys/cpu.h"
#include "sys/dw.h"
#include "sys/interrupt.h"

void sched_thread_schedule(thread_t *thread) {
    thread->state = THREAD_STATE_READY;

    spinlock_acquire_nodw(&thread->scheduler->lock);
    list_push_back(&thread->scheduler->thread_queue, &thread->list_sched);
    spinlock_release_nodw(&thread->scheduler->lock);
}

thread_t *sched_thread_next(sched_t *sched) {
    spinlock_acquire_nodw(&sched->lock);
    if(sched->thread_queue.count == 0) {
        spinlock_release_nodw(&sched->lock);
        return nullptr;
    }

    thread_t *thread = CONTAINER_OF(list_pop(&sched->thread_queue), thread_t, list_sched);

    spinlock_release_nodw(&sched->lock);
    thread->state = THREAD_STATE_ACTIVE; // TODO: move this?
    return thread;
}

void sched_yield(enum thread_state yield_state) {
    interrupt_state_t previous_state = interrupt_state_mask();

    ASSERT(yield_state != THREAD_STATE_ACTIVE);
    ASSERT(!cpu_current()->flags.in_interrupt_hard && !cpu_current()->flags.in_interrupt_soft);

    thread_t *current = sched_thread_current();

    thread_t *next = sched_thread_next(&cpu_current()->sched);
    if(next == nullptr && current != current->scheduler->idle_thread) next = current->scheduler->idle_thread;
    if(next != nullptr) {
        ASSERT(current != next);
        current->state = yield_state;
        sched_context_switch(current, next);
    } else {
        ASSERT(current->state == yield_state);
    }

    sched_preempt();

    interrupt_state_restore(previous_state);
}

void sched_preempt_inc() {
    interrupt_state_t previous_state = interrupt_state_mask();
    cpu_t *current_cpu = cpu_current();
    ASSERT(current_cpu->sched.status.preempt_counter < UINT32_MAX);
    current_cpu->sched.status.preempt_counter++;
    interrupt_state_restore(previous_state);
}

void sched_preempt_dec() {
    interrupt_state_t previous_state = interrupt_state_mask();

    cpu_t *current_cpu = cpu_current();
    ASSERT(current_cpu->sched.status.preempt_counter != 0);
    current_cpu->sched.status.preempt_counter--;

    bool yield_now = false;
    if(current_cpu->sched.status.preempt_counter == 0 && current_cpu->sched.status.yield_immediately) {
        current_cpu->sched.status.yield_immediately = false;
        yield_now = true;
    }
    interrupt_state_restore(previous_state);

    if(yield_now) sched_yield(THREAD_STATE_READY);
}

void internal_sched_thread_drop(thread_t *thread) {
    ASSERT(!interrupt_state());
    ASSERT(thread->scheduler == &cpu_current()->sched);

    if(thread == thread->scheduler->idle_thread) return;

    switch(thread->state) {
        case THREAD_STATE_DESTROY:
            if(thread->proc != nullptr) {
                spinlock_acquire_nodw(&thread->proc->lock);
                list_node_delete(&thread->proc->threads, &thread->list_proc);
                if(thread->proc->threads.count == 0) {
                    reaper_queue_process(thread->proc);
                    dw_status_enable();
                    sched_preempt_dec();
                } else {
                    spinlock_release_nodw(&thread->proc->lock);
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
