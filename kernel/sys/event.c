#include "event.h"

#include "arch/cpu.h"
#include "arch/event.h"
#include "arch/time.h"
#include "common/assert.h"
#include "common/panic.h"
#include "lib/container.h"
#include "memory/heap.h"
#include "sys/interrupt.h"

static_assert(sizeof(rb_value_t) >= sizeof(time_t));

static rb_value_t rbnode_value(rb_node_t *node) {
    return (rb_value_t) CONTAINER_OF(node, event_t, rb_node)->deadline;
}

void events_process() {
    interrupt_state_t interrupt_state = interrupt_state_mask();
    rb_tree_t *events = &arch_cpu_current()->events;

    while(true) {
        rb_node_t *node = rb_search(events, 0, RB_SEARCH_TYPE_NEAREST);
        if(node == nullptr) break;

        event_t *event = CONTAINER_OF(node, event_t, rb_node);
        time_t time_current = arch_time_monotonic();
        if(event->deadline > time_current) {
            arch_event_timer_arm(event->deadline - time_current);
            break;
        }
        rb_remove(events, node);

        event->handler(event->data);
        heap_free(event, sizeof(event_t));
    }

    interrupt_state_restore(interrupt_state);
}

rb_tree_t event_queue_make() {
    return RB_TREE_INIT(rbnode_value);
}

void event_queue(time_t delay, event_handler_t handler, void *data) {
    ASSERT(delay > 0);

    interrupt_state_t interrupt_state = interrupt_state_mask();
    rb_tree_t *events = &arch_cpu_current()->events;

    event_t *event = heap_alloc(sizeof(event_t));
    event->handler = handler;
    event->data = data;

    time_t current_time = arch_time_monotonic();
    event->deadline = current_time + delay;

    rb_node_t *node = rb_search(events, 0, RB_SEARCH_TYPE_NEAREST);
    if(node == nullptr || rbnode_value(node) > event->deadline) arch_event_timer_arm(event->deadline - current_time);

    rb_insert(events, &event->rb_node);

    interrupt_state_restore(interrupt_state);
}

void event_cancel(event_t *event) {
    interrupt_state_t interrupt_state = interrupt_state_mask();
    rb_tree_t *events = &arch_cpu_current()->events;

    rb_remove(events, &event->rb_node);
    heap_free(event, sizeof(event_t));

    rb_node_t *first_node = rb_search(events, 0, RB_SEARCH_TYPE_NEAREST);
    if(first_node != nullptr) arch_event_timer_arm(CONTAINER_OF(first_node, event_t, rb_node)->deadline - arch_time_monotonic());

    interrupt_state_restore(interrupt_state);
}
