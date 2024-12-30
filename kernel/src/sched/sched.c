#include "sched.h"

#include "arch/sched.h"
#include "common/spinlock.h"
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

    spinlock_acquire(&g_sched_processes_lock);
    list_append(&g_sched_processes, &proc->list_sched);
    spinlock_release(&g_sched_processes_lock);
    return proc;
}

void sched_process_destroy(process_t *proc) {
    heap_free(proc);
}

void sched_thread_schedule(thread_t *thread) {
    spinlock_acquire(&g_sched_threads_lock);
    list_prepend(&g_sched_threads_queued, &thread->list_sched);
    spinlock_release(&g_sched_threads_lock);
}

thread_t *sched_thread_next() {
    spinlock_acquire(&g_sched_threads_lock);
    if(list_is_empty(&g_sched_threads_queued)) {
        spinlock_release(&g_sched_threads_lock);
        return 0;
    }
    thread_t *thread = LIST_CONTAINER_GET(g_sched_threads_queued.next, thread_t, list_sched);
    list_delete(&thread->list_sched);
    spinlock_release(&g_sched_threads_lock);
    return thread;
}

void sched_thread_drop(thread_t *thread) {
    if(thread == cpu_current()->idle_thread) return;
    if(thread->state == THREAD_STATE_DESTROY) {
        arch_sched_thread_destroy(thread);
        return;
    }
    sched_thread_schedule(thread);
}