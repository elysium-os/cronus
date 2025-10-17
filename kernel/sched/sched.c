#include "sched/sched.h"

#include "arch/cpu.h"
#include "arch/interrupt.h"
#include "arch/sched.h"
#include "common/assert.h"
#include "common/lock/spinlock.h"
#include "lib/barrier.h"
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
    list_push_back(&thread->scheduler->thread_queue, &thread->list_node_sched);
    spinlock_release_nodw(&thread->scheduler->lock);
}

thread_t *sched_thread_next(sched_t *sched) {
    spinlock_acquire_nodw(&sched->lock);
    if(sched->thread_queue.count == 0) {
        spinlock_release_nodw(&sched->lock);
        return nullptr;
    }

    thread_t *thread = CONTAINER_OF(list_pop(&sched->thread_queue), thread_t, list_node_sched);

    spinlock_release_nodw(&sched->lock);
    thread->state = THREAD_STATE_ACTIVE; // TODO: move this?
    return thread;
}

void sched_yield(enum thread_state yield_state) {
    interrupt_state_t previous_state = interrupt_state_mask();

    ASSERT(yield_state != THREAD_STATE_ACTIVE);
    ASSERT(ARCH_CPU_CURRENT_READ(sched.status.preempt_counter) == 0);
    ASSERT(ARCH_CPU_CURRENT_READ(flags.deferred_work_status) == 0);
    ASSERT(!ARCH_CPU_CURRENT_READ(flags.in_interrupt_hard) && !ARCH_CPU_CURRENT_READ(flags.in_interrupt_soft));

    thread_t *current = arch_sched_thread_current();

    thread_t *next = sched_thread_next(&ARCH_CPU_CURRENT_PTR()->sched);
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

void sched_preempt_inc() {
    ASSERT(ARCH_CPU_CURRENT_READ(sched.status.preempt_counter) < UINT32_MAX);
    ARCH_CPU_CURRENT_INC(sched.status.preempt_counter);
    BARRIER;
}

void sched_preempt_dec() {
    BARRIER;
    size_t count = ARCH_CPU_CURRENT_READ(sched.status.preempt_counter);
    ASSERT(count > 0);
    bool do_yield = count == 1 && ARCH_CPU_CURRENT_EXCHANGE(sched.status.yield_immediately, false);
    ARCH_CPU_CURRENT_DEC(sched.status.preempt_counter);
    if(do_yield) sched_yield(THREAD_STATE_READY); // FLIMSY: not sure if we need to account for the RC
}

void internal_sched_thread_drop(thread_t *thread) {
    ASSERT(!arch_interrupt_state());
    ASSERT(thread->scheduler == &ARCH_CPU_CURRENT_PTR()->sched);

    if(thread == thread->scheduler->idle_thread) return;

    switch(thread->state) {
        case THREAD_STATE_DESTROY:
            if(thread->proc != nullptr) {
                spinlock_acquire_nodw(&thread->proc->lock);
                list_node_delete(&thread->proc->threads, &thread->list_node_proc);
                if(thread->proc->threads.count == 0) {
                    reaper_queue_process(thread->proc);
                    dw_status_enable();
                    sched_preempt_dec();
                } else {
                    spinlock_release_nodw(&thread->proc->lock);
                }
            }
            reaper_queue_thread(thread);
            break;
        case THREAD_STATE_BLOCK: break;
        case THREAD_STATE_READY: sched_thread_schedule(thread); break;
        default:                 ASSERT_UNREACHABLE_COMMENT("invalid state on drop"); break;
    }
}
