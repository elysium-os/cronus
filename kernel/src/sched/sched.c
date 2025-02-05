#include "sched.h"

#include "arch/cpu.h"
#include "arch/sched.h"
#include "common/lock/spinlock.h"
#include "lib/list.h"
#include "lib/mem.h"
#include "memory/heap.h"
#include "sched/thread.h"
#include "sys/cpu.h"

#define DEFAULT_RESOURCE_COUNT 256

static long g_next_pid = 1;

static spinlock_t g_sched_processes_lock = SPINLOCK_INIT;
list_t g_sched_processes = LIST_INIT;

static spinlock_t g_sched_threads_lock = SPINLOCK_INIT;
list_t g_sched_threads_queued = LIST_INIT_CIRCULAR(g_sched_threads_queued);

process_t *sched_process_create(vm_address_space_t *address_space) {
    process_t *proc = heap_alloc(sizeof(process_t));
    proc->id = __atomic_fetch_add(&g_next_pid, 1, __ATOMIC_RELAXED);
    proc->lock = SPINLOCK_INIT;
    proc->threads = LIST_INIT;
    proc->address_space = address_space;

    ipl_t previous_ipl = spinlock_acquire(&g_sched_processes_lock);
    list_append(&g_sched_processes, &proc->list_sched);
    spinlock_release(&g_sched_processes_lock, previous_ipl);
    return proc;
}

void sched_process_destroy(process_t *proc) {
    heap_free(proc, sizeof(process_t));
}

void sched_thread_schedule(thread_t *thread) {
    thread->state = THREAD_STATE_READY;
    ipl_t previous_ipl = spinlock_acquire(&g_sched_threads_lock);
    list_prepend(&g_sched_threads_queued, &thread->list_sched);
    spinlock_release(&g_sched_threads_lock, previous_ipl);
}

thread_t *sched_thread_next() {
    ipl_t previous_ipl = spinlock_acquire(&g_sched_threads_lock);
    if(list_is_empty(&g_sched_threads_queued)) {
        spinlock_release(&g_sched_threads_lock, previous_ipl);
        return NULL;
    }
    thread_t *thread = LIST_CONTAINER_GET(g_sched_threads_queued.next, thread_t, list_sched);
    list_delete(&thread->list_sched);
    spinlock_release(&g_sched_threads_lock, previous_ipl);
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
