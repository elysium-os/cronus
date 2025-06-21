#pragma once

#include "lib/list.h"

typedef void (*dw_function_t)(void *data);

typedef struct {
    dw_function_t fn;
    void *data;
    list_node_t list_node;
} dw_item_t;

/// Queue an item of deferred work.
void dw_queue(dw_function_t fn, void *data);

/// Process deferred work.
void dw_process();

/// Initialize deferred work system.
void dw_init();
