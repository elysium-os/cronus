#pragma once

#include "memory/vm.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initialize the heap.
 */
void heap_initialize(vm_address_space_t *address_space, size_t size);

/**
 * @brief Allocate a block of memory in the heap, without an alignment.
 */
void *heap_alloc(size_t size);

/**
 * @brief Allocate a block of memory in the heap, with an alignment.
 */
void *heap_alloc_align(size_t size, size_t alignment);

/**
 * @brief Free a block of memory in the heap.
 */
void heap_free(void *address);
