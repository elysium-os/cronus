#pragma once

#include <stddef.h>

/// Initialize the heap.
void heap_initialize();

/// Allocate a block of memory.
void *heap_alloc(size_t size);

/// Reallocate a block of memory with a new size.
void *heap_realloc(void *address, size_t current_size, size_t new_size);

/// Reallocate a block of memory in the form of an array.
void *heap_reallocarray(void *array, size_t element_size, size_t current_count, size_t new_count);

/// Free a block of memory.
void heap_free(void *address, size_t size);
