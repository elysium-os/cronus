#include "reaper.h"

#include "common/lock/spinlock.h"
#include "lib/list.h"
#include "sys/interrupt.h"

static spinlock_t g_reaper_lock = SPINLOCK_INIT;
static list_t g_thread_queue = LIST_INIT_CIRCULAR(g_thread_queue);
static list_t g_process_queue = LIST_INIT_CIRCULAR(g_process_queue);

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
