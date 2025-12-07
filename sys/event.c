#include "sys/event.h"

#include "arch/event.h"
#include "arch/time.h"
#include "common/assert.h"
#include "common/attr.h"
#include "lib/container.h"
#include "memory/slab.h"
#include "sched/sched.h"
#include "sys/cpu.h"
#include "sys/dw.h"
#include "sys/init.h"
#include "sys/interrupt.h"

static_assert(sizeof(rb_value_t) >= sizeof(time_t));

static rb_value_t rbnode_value(rb_node_t *node);

static slab_cache_t *g_event_cache;
ATTR(cpu_local) static rb_tree_t gc_events = RB_TREE_INIT(rbnode_value);
ATTR(cpu_local) static rb_tree_t gc_free_events = RB_TREE_INIT(rbnode_value); /* Events to be free'd */

static rb_value_t rbnode_value(rb_node_t *node) {
    return (rb_value_t) CONTAINER_OF(node, event_t, rb_node)->deadline;
}

void events_process(arch_interrupt_frame_t *) {
    sched_preempt_inc();

    while(true) {
        rb_node_t *node = rb_search(CPU_LOCAL_PTR(gc_events, rb_tree_t), 0, RB_SEARCH_TYPE_NEAREST);
        if(node == nullptr) break;

        event_t *event = CONTAINER_OF(node, event_t, rb_node);
        time_t time_current = arch_time_monotonic();
        if(event->deadline > time_current) {
            arch_event_timer_arm(event->deadline - time_current);
            break;
        }

        rb_remove(CPU_LOCAL_PTR(gc_events, rb_tree_t), node);
        dw_queue(event->dw_item);

        if(gc_cpu_flags.in_interrupt_hard) {
            rb_insert(CPU_LOCAL_PTR(gc_free_events, rb_tree_t), &event->rb_node);
        } else {
            slab_free(g_event_cache, event);
        }
    }

    sched_preempt_dec();
}

void event_queue(time_t delay, dw_function_t fn, void *data) {
    ASSERT(delay > 0);

    interrupt_state_t interrupt_state = interrupt_state_mask();

    event_t *event;
    if(gc_free_events.count != 0) {
        rb_node_t *node = rb_search(CPU_LOCAL_PTR(gc_free_events, rb_tree_t), 0, RB_SEARCH_TYPE_NEAREST);
        ASSERT(node != nullptr);
        rb_remove(CPU_LOCAL_PTR(gc_free_events, rb_tree_t), node);
        event = CONTAINER_OF(node, event_t, rb_node);

        while(gc_free_events.count != 0) {
            rb_node_t *node = rb_search(CPU_LOCAL_PTR(gc_free_events, rb_tree_t), 0, RB_SEARCH_TYPE_NEAREST);
            rb_remove(CPU_LOCAL_PTR(gc_free_events, rb_tree_t), node);
            slab_free(g_event_cache, CONTAINER_OF(node, event_t, rb_node));
        }
    } else {
        event = slab_allocate(g_event_cache);
    }
    event->dw_item = dw_create(fn, data);

    time_t current_time = arch_time_monotonic();
    event->deadline = current_time + delay;

    rb_node_t *node = rb_search(CPU_LOCAL_PTR(gc_events, rb_tree_t), 0, RB_SEARCH_TYPE_NEAREST);
    if(node == nullptr || rbnode_value(node) > event->deadline) arch_event_timer_arm(event->deadline - current_time);

    rb_insert(CPU_LOCAL_PTR(gc_events, rb_tree_t), &event->rb_node);

    interrupt_state_restore(interrupt_state);
}

void event_cancel(event_t *event) {
    sched_preempt_inc();

    rb_remove(CPU_LOCAL_PTR(gc_events, rb_tree_t), &event->rb_node);
    slab_free(g_event_cache, event);

    rb_node_t *first_node = rb_search(CPU_LOCAL_PTR(gc_events, rb_tree_t), 0, RB_SEARCH_TYPE_NEAREST);
    if(first_node != nullptr) arch_event_timer_arm(CONTAINER_OF(first_node, event_t, rb_node)->deadline - arch_time_monotonic());

    sched_preempt_dec();
}

static void event_cache_init() {
    g_event_cache = slab_cache_create("event", sizeof(event_t), 2);
}

INIT_TARGET(event_cache, INIT_STAGE_MAIN, event_cache_init);
