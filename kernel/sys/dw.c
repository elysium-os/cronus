#include "sys/dw.h"

#include "arch/cpu.h"
#include "common/assert.h"
#include "lib/barrier.h"
#include "lib/container.h"
#include "memory/slab.h"
#include "sys/init.h"

#include <stdint.h>

static slab_cache_t *g_item_cache;

dw_item_t *dw_create(dw_function_t fn, void *data) {
    dw_item_t *item = slab_allocate(g_item_cache);
    item->fn = fn;
    item->data = data;
    return item;
}

void dw_queue(dw_item_t *item) {
    sched_preempt_inc();
    list_push(&CPU_CURRENT_PTR()->dw_items, &item->list_node);
    sched_preempt_dec();
}

void dw_process() {
repeat:
    sched_preempt_inc();
    cpu_t *current_cpu = CPU_CURRENT_PTR();
    if(current_cpu->dw_items.count == 0) {
        sched_preempt_dec();
        return;
    }
    dw_item_t *dw_item = CONTAINER_OF(list_pop(&current_cpu->dw_items), dw_item_t, list_node);
    sched_preempt_dec();

    dw_item->fn(dw_item->data);

    slab_free(g_item_cache, dw_item);
    goto repeat;
}

void dw_status_disable() {
    ASSERT(CPU_CURRENT_READ(flags.deferred_work_status) < UINT32_MAX);
    CPU_CURRENT_INC(flags.deferred_work_status);
    BARRIER;
}

void dw_status_enable() {
    BARRIER;
    size_t status = CPU_CURRENT_READ(flags.deferred_work_status);
    ASSERT(status > 0);
    CPU_CURRENT_DEC(flags.deferred_work_status);
    if(status == 1) dw_process();
}

static void dw_init() {
    g_item_cache = slab_cache_create("deferred_work", sizeof(dw_item_t), 2);
}

INIT_TARGET(deferred_work, INIT_STAGE_MAIN, dw_init);
