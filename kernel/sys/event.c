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

void events_process() {
    interrupt_state_t interrupt_state = interrupt_state_mask();
    cpu_t *current_cpu = cpu_current();

    while(true) {
        rb_node_t *node = rb_search(&current_cpu->events, 0, RB_SEARCH_TYPE_NEAREST);
        if(node == nullptr) break;

        event_t *event = CONTAINER_OF(node, event_t, rb_node);
        time_t time_current = time_monotonic();
        if(event->deadline > time_current) {
            event_timer_arm(event->deadline - time_current);
            break;
        }

        rb_remove(&current_cpu->events, node);
        dw_queue(event->dw_item);

        if(cpu_current()->flags.in_interrupt_hard) {
            rb_insert(&current_cpu->free_events, &event->rb_node);
        } else {
            slab_free(g_event_cache, event);
        }
    }

    interrupt_state_restore(interrupt_state);
}

void event_queue(time_t delay, dw_function_t fn, void *data) {
    ASSERT(delay > 0);

    interrupt_state_t interrupt_state = interrupt_state_mask();
    cpu_t *current_cpu = cpu_current();

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

    time_t current_time = time_monotonic();
    event->deadline = current_time + delay;

    rb_node_t *node = rb_search(&current_cpu->events, 0, RB_SEARCH_TYPE_NEAREST);
    if(node == nullptr || rbnode_value(node) > event->deadline) event_timer_arm(event->deadline - current_time);

    rb_insert(&current_cpu->events, &event->rb_node);

    interrupt_state_restore(interrupt_state);
}

void event_cancel(event_t *event) {
    interrupt_state_t interrupt_state = interrupt_state_mask();
    rb_tree_t *events = &cpu_current()->events;

    rb_remove(events, &event->rb_node);
    slab_free(g_event_cache, event);

    rb_node_t *first_node = rb_search(events, 0, RB_SEARCH_TYPE_NEAREST);
    if(first_node != nullptr) event_timer_arm(CONTAINER_OF(first_node, event_t, rb_node)->deadline - time_monotonic());

    interrupt_state_restore(interrupt_state);
}

void event_init_cpu_local() {
    interrupt_state_t prev_state = interrupt_state_mask();
    cpu_t *current_cpu = cpu_current();
    current_cpu->events = RB_TREE_INIT(rbnode_value);
    current_cpu->free_events = RB_TREE_INIT(rbnode_value);
    interrupt_state_restore(prev_state);
}

static void event_cache_init() {
    g_event_cache = slab_cache_create("event", sizeof(event_t), 2);
}

INIT_TARGET(event_cache, INIT_STAGE_MAIN, event_cache_init);
