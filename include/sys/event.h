#pragma once

#include "arch/interrupt.h"
#include "lib/rb.h"
#include "sys/dw.h"
#include "sys/time.h"

typedef struct {
    time_t deadline;
    rb_node_t rb_node;
    dw_item_t *dw_item;
} event_t;

/// Process outstanding events and rearm.
void events_process(arch_interrupt_frame_t *frame);

/// Queue an event.
void event_queue(time_t delay, dw_function_t fn, void *data);
