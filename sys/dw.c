#include "sys/dw.h"

#include "arch/cpu.h"
#include "common/assert.h"
#include "lib/barrier.h"
#include "lib/container.h"
#include "memory/slab.h"
#include "sys/hook.h"

#include <stdint.h>

static slab_cache_t *g_item_cache;

static bool dw_enable() {
    BARRIER;
    size_t status = ARCH_CPU_CURRENT_READ(flags.deferred_work_status);
    ASSERT(status > 0);
    ARCH_CPU_CURRENT_DEC(flags.deferred_work_status);
    return status == 1;
}

static void cleanup_created_dw(dw_item_t *item) {
    slab_free(g_item_cache, item);
}

dw_item_t *dw_create(dw_function_t fn, void *data) {
    dw_item_t *item = slab_allocate(g_item_cache);
    item->fn = fn;
    item->data = data;
    item->cleanup_fn = cleanup_created_dw;
    return item;
}

void dw_queue(dw_item_t *item) {
    sched_preempt_inc();
    dw_status_disable();
    list_push(&ARCH_CPU_CURRENT_PTR()->dw_items, &item->list_node);
    dw_enable();
    sched_preempt_dec();
}

void dw_process() {
    dw_status_disable();
repeat:
    sched_preempt_inc();
    cpu_t *current_cpu = ARCH_CPU_CURRENT_PTR();
    if(current_cpu->dw_items.count == 0) {
        sched_preempt_dec();
        dw_enable();
        return;
    }
    interrupt_state_t prev_state = interrupt_state_mask();
    dw_item_t *dw_item = CONTAINER_OF(list_pop(&current_cpu->dw_items), dw_item_t, list_node);
    interrupt_state_restore(prev_state);
    sched_preempt_dec();

    dw_item->fn(dw_item->data);

    if(dw_item->cleanup_fn != nullptr) dw_item->cleanup_fn(dw_item);

    goto repeat;
}

void dw_status_disable() {
    ASSERT(ARCH_CPU_CURRENT_READ(flags.deferred_work_status) < UINT32_MAX);
    ARCH_CPU_CURRENT_INC(flags.deferred_work_status);
    BARRIER;
}

void dw_status_enable() {
    if(dw_enable()) dw_process();
}

HOOK(init_slab_cache) {
    g_item_cache = slab_cache_create("deferred_work", sizeof(dw_item_t), 2);
}
