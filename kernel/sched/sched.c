#include "sched.h"

#include "arch/cpu.h"
#include "common/assert.h"
#include "common/lock/spinlock.h"
#include "lib/list.h"
#include "reaper.h"
#include "sched/thread.h"
#include "sys/cpu.h"

static spinlock_t g_scheduler_lock = SPINLOCK_INIT;

static list_t g_sched_threads_queued = LIST_INIT_CIRCULAR(g_sched_threads_queued);

void sched_thread_schedule(thread_t *thread) {
    thread->state = THREAD_STATE_READY;
    interrupt_state_t previous_state = spinlock_acquire(&g_scheduler_lock);
    list_prepend(&g_sched_threads_queued, &thread->list_sched);
    spinlock_release(&g_scheduler_lock, previous_state);
}

thread_t *internal_sched_thread_next() {
    interrupt_state_t previous_state = spinlock_acquire(&g_scheduler_lock);
    if(list_is_empty(&g_sched_threads_queued)) {
        spinlock_release(&g_scheduler_lock, previous_state);
        return NULL;
    }
    thread_t *thread = LIST_CONTAINER_GET(LIST_NEXT(&g_sched_threads_queued), thread_t, list_sched);
    list_delete(&thread->list_sched);
    spinlock_release(&g_scheduler_lock, previous_state);
    thread->state = THREAD_STATE_ACTIVE;
    return thread;
}

void internal_sched_thread_drop(thread_t *thread) {
    if(thread == arch_cpu_current()->idle_thread) return;

    switch(thread->state) {
        case THREAD_STATE_DESTROY:
            if(thread->proc != NULL) {
                interrupt_state_t previous_state = spinlock_acquire(&thread->proc->lock);
                list_delete(&thread->list_proc);
                if(list_is_empty(&thread->proc->threads)) {
                    process_destroy(thread->proc);
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
