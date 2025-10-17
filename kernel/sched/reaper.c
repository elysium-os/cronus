#include "sched/reaper.h"

#include "arch/sched.h"
#include "common/assert.h"
#include "common/lock/spinlock.h"
#include "common/log.h"
#include "lib/container.h"
#include "lib/list.h"
#include "memory/heap.h"
#include "sched/process.h"
#include "sched/thread.h"

static spinlock_t g_reaper_lock = SPINLOCK_INIT;
static list_t g_thread_queue = LIST_INIT;
static list_t g_process_queue = LIST_INIT;

static void reaper_thread() {
repeat:

    while(true) {
        spinlock_acquire_nodw(&g_reaper_lock);
        if(g_process_queue.count == 0) {
            spinlock_release_nodw(&g_reaper_lock);
            break;
        }
        process_t *process = CONTAINER_OF(list_pop(&g_process_queue), process_t, list_sched);
        spinlock_release_nodw(&g_reaper_lock);

        ASSERT(!spinlock_try_acquire(&process->lock));
        ASSERT(process->threads.count == 0);

        log(LOG_LEVEL_DEBUG, "REAPER", "pid: %lu", process->id);

        // TODO: free process address space
        heap_free(process, sizeof(process_t));
    }

    while(true) {
        spinlock_acquire_nodw(&g_reaper_lock);
        if(g_thread_queue.count == 0) {
            spinlock_release_nodw(&g_reaper_lock);
            break;
        }
        thread_t *thread = CONTAINER_OF(list_pop(&g_thread_queue), thread_t, list_node_sched);
        spinlock_release_nodw(&g_reaper_lock);

        log(LOG_LEVEL_DEBUG, "REAPER", "tid: %lu", thread->id);

        // TODO: free arch thread
    }

    sched_yield(THREAD_STATE_BLOCK);
    goto repeat;
    ASSERT_UNREACHABLE();
}

thread_t *reaper_create() {
    return arch_sched_thread_create_kernel(reaper_thread);
}

void reaper_queue_thread(thread_t *thread) {
    spinlock_acquire_nodw(&g_reaper_lock);
    list_push_back(&g_thread_queue, &thread->list_node_sched);
    spinlock_release_nodw(&g_reaper_lock);
}

void reaper_queue_process(process_t *process) {
    spinlock_acquire_nodw(&g_reaper_lock);
    list_push_back(&g_process_queue, &process->list_sched);
    spinlock_release_nodw(&g_reaper_lock);
}
