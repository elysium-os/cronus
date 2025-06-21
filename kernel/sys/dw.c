#include "dw.h"

#include "arch/cpu.h"
#include "lib/container.h"
#include "memory/slab.h"

static slab_cache_t *g_item_cache;

void dw_queue(dw_function_t fn, void *data) {
    dw_item_t *item = slab_allocate(g_item_cache);
    item->fn = fn;
    item->data = data;

    interrupt_state_t previous_state = interrupt_state_mask();
    cpu_t *current_cpu = arch_cpu_current();
    list_push(&current_cpu->dw_items, &item->list_node);
    interrupt_state_restore(previous_state);
}

void dw_process() {
repeat:
    interrupt_state_t previous_state = interrupt_state_mask();
    cpu_t *current_cpu = arch_cpu_current();
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

void dw_init() {
    g_item_cache = slab_cache_create("deferred_work", sizeof(dw_item_t), 2);
}
