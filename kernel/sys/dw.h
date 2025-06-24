#pragma once

#include "lib/list.h"

typedef void (*dw_function_t)(void *data);

typedef struct {
    dw_function_t fn;
    void *data;
    list_node_t list_node;
} dw_item_t;

/// Create an item of deferred work.
dw_item_t *dw_create(dw_function_t fn, void *data);

/// Queue an item of deferred work.
void dw_queue(dw_item_t *item);

/// Process deferred work.
void dw_process();

/// Disable deferred work (increment the status counter).
void dw_status_disable();

/// Enable deferred work. Note that deferred work is only actually
/// enabled when the status reaches zero.
void dw_status_enable();

/// Initialize deferred work system.
void dw_init();
