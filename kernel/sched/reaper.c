#include "reaper.h"

#include "arch/sched.h"
#include "common/assert.h"
#include "common/lock/spinlock.h"
#include "common/log.h"
#include "lib/list.h"
#include "memory/heap.h"
#include "sched/process.h"
#include "sched/thread.h"
#include "sys/interrupt.h"

static spinlock_t g_reaper_lock = SPINLOCK_INIT;
static list_t g_thread_queue = LIST_INIT_CIRCULAR(g_thread_queue);
static list_t g_process_queue = LIST_INIT_CIRCULAR(g_process_queue);

static void reaper_thread() {
repeat:

    while(true) {
        interrupt_state_t previous_state = spinlock_acquire(&g_reaper_lock);
        if(list_is_empty(&g_process_queue)) {
            spinlock_release(&g_reaper_lock, previous_state);
            break;
        }
        process_t *process = LIST_CONTAINER_GET(LIST_NEXT(&g_process_queue), process_t, list_sched);
        list_delete(&process->list_sched);
        spinlock_release(&g_reaper_lock, previous_state);

        ASSERT(!spinlock_acquire(&process->lock));
        ASSERT(list_is_empty(&process->threads));

        log(LOG_LEVEL_DEBUG, "REAPER", "pid: %lu", process->id);

        // TODO: free process address space
        heap_free(process, sizeof(process_t));
    }

    while(true) {
        interrupt_state_t previous_state = spinlock_acquire(&g_reaper_lock);
        if(list_is_empty(&g_thread_queue)) {
            spinlock_release(&g_reaper_lock, previous_state);
            break;
        }
        thread_t *thread = LIST_CONTAINER_GET(LIST_NEXT(&g_thread_queue), thread_t, list_sched);
        list_delete(&thread->list_sched);
        spinlock_release(&g_reaper_lock, previous_state);

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
    interrupt_state_t previous_state = spinlock_acquire(&g_reaper_lock);
    list_prepend(&g_thread_queue, &thread->list_sched);
    spinlock_release(&g_reaper_lock, previous_state);
}

void reaper_queue_process(process_t *process) {
    interrupt_state_t previous_state = spinlock_acquire(&g_reaper_lock);
    list_prepend(&g_process_queue, &process->list_sched);
    spinlock_release(&g_reaper_lock, previous_state);
}
