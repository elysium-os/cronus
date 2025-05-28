#pragma once

#include "lib/rb.h"
#include "sys/time.h"

typedef void (*event_handler_t)(void *data);

typedef struct {
    time_t deadline;
    rb_node_t rb_node;

    event_handler_t handler;
    void *data;
} event_t;

/// Process outstanding events and rearm.
void events_process();

/// Create an event queue, used to initialize CPU local.
rb_tree_t event_queue_make();

/// Queue an event.
void event_queue(time_t delay, event_handler_t handler, void *data);
