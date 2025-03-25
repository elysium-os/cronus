#pragma once

#include "common/lock/spinlock.h"
#include "lib/list.h"
#include "memory/vm.h"

typedef struct {
    long id;
    spinlock_t lock;
    vm_address_space_t *address_space;
    list_t threads;
    list_element_t list_sched; /* used by scheduler/reaper */
} process_t;

/**
 * @brief Create a process.
 */
process_t *process_create(vm_address_space_t *address_space);

/*
 * @brief Destroy a process.
 */
void process_destroy(process_t *process);
