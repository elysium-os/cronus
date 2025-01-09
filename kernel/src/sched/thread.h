#pragma once

#include "lib/list.h"
#include "sched/process.h"
#include "sys/cpu.h"

typedef enum {
    THREAD_STATE_READY,
    THREAD_STATE_ACTIVE,
    THREAD_STATE_BLOCK,
    THREAD_STATE_DESTROY
} thread_state_t;

typedef struct thread {
    long id;
    thread_state_t state;
    process_t *proc;

    list_element_t list_sched; /* list used by scheduler */
    list_element_t list_proc; /* list used by process */
    list_element_t list_wait; /* list used by waitable */
} thread_t;
