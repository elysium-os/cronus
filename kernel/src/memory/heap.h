#pragma once

#include "memory/vm.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initialize the heap.
 */
void heap_initialize();

/**
 * @brief Allocate a block of memory in the heap, without an alignment.
 */
void *heap_alloc(size_t size);

/**
 * @brief Free a block of memory in the heap.
 */
void heap_free(void *address, size_t size);
