#include "sys/dw.h"

#include "arch/cpu.h"
#include "common/assert.h"
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
    interrupt_state_t previous_state = interrupt_state_mask();
    cpu_t *current_cpu = cpu_current();
    list_push(&current_cpu->dw_items, &item->list_node);
    interrupt_state_restore(previous_state);
}

void dw_process() {
    interrupt_state_t previous_state = interrupt_state_mask();
repeat:
    cpu_t *current_cpu = cpu_current();
    if(current_cpu->dw_items.count == 0) {
        interrupt_state_restore(previous_state);
        return;
    }
    dw_item_t *dw_item = CONTAINER_OF(list_pop(&current_cpu->dw_items), dw_item_t, list_node);
    interrupt_state_restore(previous_state);

    dw_item->fn(dw_item->data);

    slab_free(g_item_cache, dw_item);
    goto repeat;
}

void dw_status_disable() {
    interrupt_state_t previous_state = interrupt_state_mask();
    cpu_t *current_cpu = cpu_current();
    ASSERT(current_cpu->flags.deferred_work_status < UINT32_MAX);
    current_cpu->flags.deferred_work_status++;
    interrupt_state_restore(previous_state);
}

void dw_status_enable() {
    interrupt_state_t previous_state = interrupt_state_mask();
    cpu_t *current_cpu = cpu_current();
    current_cpu->flags.deferred_work_status--;
    bool process_work = current_cpu->flags.deferred_work_status == 0;
    interrupt_state_restore(previous_state);
    if(process_work) dw_process();
}

static void dw_init() {
    g_item_cache = slab_cache_create("deferred_work", sizeof(dw_item_t), 2);
}

INIT_TARGET(deferred_work, INIT_STAGE_MAIN, dw_init);
