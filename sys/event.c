#include "sys/event.h"

#include "arch/cpu.h"
#include "arch/event.h"
#include "arch/time.h"
#include "common/assert.h"
#include "lib/container.h"
#include "memory/slab.h"
#include "sys/dw.h"
#include "sys/init.h"
#include "sys/interrupt.h"

static_assert(sizeof(rb_value_t) >= sizeof(time_t));

static slab_cache_t *g_event_cache;

static rb_value_t rbnode_value(rb_node_t *node) {
    return (rb_value_t) CONTAINER_OF(node, event_t, rb_node)->deadline;
}

void events_process(arch_interrupt_frame_t *) {
    sched_preempt_inc();
    cpu_t *current_cpu = ARCH_CPU_CURRENT_PTR();

    while(true) {
        rb_node_t *node = rb_search(&current_cpu->events, 0, RB_SEARCH_TYPE_NEAREST);
        if(node == nullptr) break;

        event_t *event = CONTAINER_OF(node, event_t, rb_node);
        time_t time_current = arch_time_monotonic();
        if(event->deadline > time_current) {
            arch_event_timer_arm(event->deadline - time_current);
            break;
        }

        rb_remove(&current_cpu->events, node);
        dw_queue(event->dw_item);

        if(ARCH_CPU_CURRENT_READ(flags.in_interrupt_hard)) {
            rb_insert(&current_cpu->free_events, &event->rb_node);
        } else {
            slab_free(g_event_cache, event);
        }
    }

    sched_preempt_dec();
}

void event_queue(time_t delay, dw_function_t fn, void *data) {
    ASSERT(delay > 0);

    interrupt_state_t interrupt_state = interrupt_state_mask();
    cpu_t *current_cpu = ARCH_CPU_CURRENT_PTR();

    event_t *event;
    if(current_cpu->free_events.count != 0) {
        rb_node_t *node = rb_search(&current_cpu->free_events, 0, RB_SEARCH_TYPE_NEAREST);
        ASSERT(node != nullptr);
        rb_remove(&current_cpu->free_events, node);
        event = CONTAINER_OF(node, event_t, rb_node);

        while(current_cpu->free_events.count != 0) {
            rb_node_t *node = rb_search(&current_cpu->free_events, 0, RB_SEARCH_TYPE_NEAREST);
            rb_remove(&current_cpu->free_events, node);
            slab_free(g_event_cache, CONTAINER_OF(node, event_t, rb_node));
        }
    } else {
        event = slab_allocate(g_event_cache);
    }
    event->dw_item = dw_create(fn, data);

    time_t current_time = arch_time_monotonic();
    event->deadline = current_time + delay;

    rb_node_t *node = rb_search(&current_cpu->events, 0, RB_SEARCH_TYPE_NEAREST);
    if(node == nullptr || rbnode_value(node) > event->deadline) arch_event_timer_arm(event->deadline - current_time);

    rb_insert(&current_cpu->events, &event->rb_node);

    interrupt_state_restore(interrupt_state);
}

void event_cancel(event_t *event) {
    sched_preempt_inc();
    rb_tree_t *events = &ARCH_CPU_CURRENT_PTR()->events;

    rb_remove(events, &event->rb_node);
    slab_free(g_event_cache, event);

    rb_node_t *first_node = rb_search(events, 0, RB_SEARCH_TYPE_NEAREST);
    if(first_node != nullptr) arch_event_timer_arm(CONTAINER_OF(first_node, event_t, rb_node)->deadline - arch_time_monotonic());

    sched_preempt_dec();
}

void event_init_cpu_local() {
    sched_preempt_inc();
    ARCH_CPU_CURRENT_PTR()->events = RB_TREE_INIT(rbnode_value);
    ARCH_CPU_CURRENT_PTR()->free_events = RB_TREE_INIT(rbnode_value);
    sched_preempt_dec();
}

INIT_TARGET(event_cache, INIT_STAGE_MAIN, INIT_SCOPE_BSP, INIT_DEPS()) {
    g_event_cache = slab_cache_create("event", sizeof(event_t), 2);
}
