#include "sched/sched.h"

#include "arch/interrupt.h"
#include "arch/sched.h"
#include "common/assert.h"
#include "common/attr.h"
#include "common/lock/spinlock.h"
#include "lib/barrier.h"
#include "lib/container.h"
#include "lib/list.h"
#include "sched/reaper.h"
#include "sched/thread.h"
#include "sys/cpu.h"
#include "sys/dw.h"
#include "sys/interrupt.h"

ATTR(cpu_local)
sched_t gc_sched = {
    .idle_thread = nullptr,
    .thread_queue = LIST_INIT,
    .lock = SPINLOCK_INIT,
    .status = { .preempt_counter = 0, .yield_immediately = false }
};

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
    ASSERT(ATOMIC_LOAD(&gc_sched.status.preempt_counter, ATOMIC_RELAXED) == 0);
    ASSERT(ATOMIC_LOAD(&gc_cpu_flags.deferred_work_status, ATOMIC_RELAXED) == 0);
    ASSERT(!ATOMIC_LOAD(&gc_cpu_flags.in_interrupt_hard, ATOMIC_RELAXED) && !ATOMIC_LOAD(&gc_cpu_flags.in_interrupt_soft, ATOMIC_RELAXED));

    thread_t *current = arch_sched_thread_current();

    thread_t *next = sched_thread_next(CPU_LOCAL_PTR(gc_sched, sched_t));
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
    size_t prev_count = ATOMIC_FETCH_ADD(&gc_sched.status.preempt_counter, 1, ATOMIC_ACQUIRE);
    ASSERT(prev_count < UINT32_MAX);
    BARRIER;
}

void sched_preempt_dec() {
    BARRIER;
    size_t count = ATOMIC_FETCH_SUB(&gc_sched.status.preempt_counter, 1, ATOMIC_RELEASE);
    ASSERT(count > 0);
    if(count == 1 && ATOMIC_EXCHANGE(&gc_sched.status.yield_immediately, false, ATOMIC_ACQUIRE)) sched_yield(THREAD_STATE_READY); // FLIMSY: not sure if we need to account for the RC
}

void internal_sched_thread_drop(thread_t *thread) {
    ASSERT(!arch_interrupt_state());
    ASSERT(thread->scheduler == CPU_LOCAL_PTR(gc_sched, sched_t));

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
