#include "sys/dw.h"

#include "common/assert.h"
#include "lib/barrier.h"
#include "lib/container.h"
#include "memory/slab.h"
#include "sched/sched.h"
#include "sys/cpu.h"
#include "sys/init.h"

#include <stdint.h>

static slab_cache_t *g_item_cache;
ATTR(cpu_local) list_t gc_dw_items;
ATTR(cpu_local, atomic) size_t gc_dw_deferred_work_status;

static bool dw_enable() {
    BARRIER;
    size_t status = ATOMIC_FETCH_SUB(&gc_dw_deferred_work_status, 1, ATOMIC_ACQUIRE);
    ASSERT(status > 0);
    return status == 1;
}

dw_item_t *dw_create(dw_function_t fn, void *data) {
    dw_item_t *item = slab_allocate(g_item_cache);
    item->fn = fn;
    item->data = data;
    return item;
}

void dw_queue(dw_item_t *item) {
    sched_preempt_inc();
    dw_status_disable();
    list_push(CPU_LOCAL_PTR(gc_dw_items, list_t), &item->list_node);
    dw_enable();
    sched_preempt_dec();
}

void dw_process() {
    dw_status_disable();
repeat:
    sched_preempt_inc();
    if(gc_dw_items.count == 0) {
        sched_preempt_dec();
        dw_enable();
        return;
    }
    dw_item_t *dw_item = CONTAINER_OF(list_pop(CPU_LOCAL_PTR(gc_dw_items, list_t)), dw_item_t, list_node);
    sched_preempt_dec();

    dw_item->fn(dw_item->data);

    slab_free(g_item_cache, dw_item);
    goto repeat;
}

void dw_status_disable() {
    size_t prev_status = ATOMIC_FETCH_ADD(&gc_dw_deferred_work_status, 1, ATOMIC_RELEASE);
    ASSERT(prev_status < UINT32_MAX);
    BARRIER;
}

void dw_status_enable() {
    if(dw_enable()) dw_process();
}

static void dw_init() {
    g_item_cache = slab_cache_create("deferred_work", sizeof(dw_item_t), 2);
}

INIT_TARGET(deferred_work, INIT_STAGE_MAIN, dw_init);
