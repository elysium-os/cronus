#pragma once

#include "lib/container.h"
#include "memory/vm.h"

#define X86_64_PTM_AS(ADDRESS_SPACE) (CONTAINER_OF((ADDRESS_SPACE), x86_64_ptm_address_space_t, common))

typedef struct {
    spinlock_t pt_lock;
    uintptr_t pt_top;
    vm_address_space_t common;
} x86_64_ptm_address_space_t;
