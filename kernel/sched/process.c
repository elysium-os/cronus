#include "process.h"

#include "common/log.h"
#include "memory/heap.h"

static long g_next_pid = 1;

static spinlock_t g_sched_processes_lock = SPINLOCK_INIT;
list_t g_sched_processes = LIST_INIT;

process_t *process_create(vm_address_space_t *address_space) {
    process_t *proc = heap_alloc(sizeof(process_t));
    proc->id = __atomic_fetch_add(&g_next_pid, 1, __ATOMIC_RELAXED);
    proc->lock = SPINLOCK_INIT;
    proc->threads = LIST_INIT;
    proc->address_space = address_space;

    interrupt_state_t previous_state = spinlock_acquire(&g_sched_processes_lock);
    list_push_back(&g_sched_processes, &proc->list_sched);
    spinlock_release(&g_sched_processes_lock, previous_state);

    log(LOG_LEVEL_DEBUG, "PROCESS", "created pid %lu", proc->id);

    return proc;
}

void process_destroy(process_t *process) {
    log(LOG_LEVEL_DEBUG, "PROCESS", "destroyed pid %lu", process->id);
}
