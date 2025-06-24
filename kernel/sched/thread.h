#pragma once

typedef enum thread_state thread_state_t;
typedef struct thread thread_t;

#include "lib/list.h"
#include "sched/process.h"
#include "sched/sched.h"
#include "sys/dw.h"

enum thread_state {
    THREAD_STATE_READY,
    THREAD_STATE_ACTIVE,
    THREAD_STATE_BLOCK,
    THREAD_STATE_DESTROY
};

struct thread {
    long id;
    thread_state_t state;
    process_t *proc;

    struct sched *scheduler;

    struct {
        bool in_flight;
        uintptr_t address;
        dw_item_t dw_item;
    } vm_fault;

    list_node_t list_sched; /* list used by scheduler/reaper */
    list_node_t list_proc; /* list used by process */
    list_node_t list_wait; /* list used by waitable */
};
