#include "sched.h"

#include "arch/cpu.h"
#include "arch/sched.h"
#include "common/lock/spinlock.h"
#include "lib/list.h"
#include "lib/mem.h"
#include "memory/heap.h"
#include "sched/thread.h"
#include "sys/cpu.h"

static spinlock_t g_sched_threads_lock = SPINLOCK_INIT;
list_t g_sched_threads_queued = LIST_INIT_CIRCULAR(g_sched_threads_queued);

void sched_thread_schedule(thread_t *thread) {
    thread->state = THREAD_STATE_READY;
    interrupt_state_t previous_state = spinlock_acquire(&g_sched_threads_lock);
    list_prepend(&g_sched_threads_queued, &thread->list_sched);
    spinlock_release(&g_sched_threads_lock, previous_state);
}

thread_t *sched_thread_next() {
    interrupt_state_t previous_state = spinlock_acquire(&g_sched_threads_lock);
    if(list_is_empty(&g_sched_threads_queued)) {
        spinlock_release(&g_sched_threads_lock, previous_state);
        return NULL;
    }
    thread_t *thread = LIST_CONTAINER_GET(g_sched_threads_queued.next, thread_t, list_sched);
    list_delete(&thread->list_sched);
    spinlock_release(&g_sched_threads_lock, previous_state);
    thread->state = THREAD_STATE_ACTIVE;
    return thread;
}

void sched_thread_drop(thread_t *thread) {
    if(thread == arch_cpu_current()->idle_thread) return;
    switch(thread->state) {
        case THREAD_STATE_DESTROY: arch_sched_thread_destroy(thread); return;
        case THREAD_STATE_BLOCK:   return;
        case THREAD_STATE_ACTIVE:  break;
        case THREAD_STATE_READY:   break;
    }
    sched_thread_schedule(thread);
}
