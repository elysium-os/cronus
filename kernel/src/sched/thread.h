#pragma once

#include "lib/list.h"
#include "sched/process.h"
#include "sys/cpu.h"

typedef enum {
    THREAD_STATE_READY,
    THREAD_STATE_ACTIVE,
    THREAD_STATE_DESTROY
} thread_state_t;

typedef struct thread {
    long id;
    thread_state_t state;
    struct cpu *cpu;
    process_t *proc;
    list_element_t list_sched;
    list_element_t list_proc;
} thread_t;
