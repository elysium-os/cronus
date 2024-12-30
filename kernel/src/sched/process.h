#pragma once

#include "common/spinlock.h"
#include "lib/list.h"
#include "memory/vm.h"

typedef struct {
    long id;
    spinlock_t lock;
    vm_address_space_t *address_space;
    list_t threads;
    list_element_t list_sched;
} process_t;