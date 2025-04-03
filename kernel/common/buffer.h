#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t size;
    uint8_t data[];
} buffer_t;

/**
 * @brief Allocate a buffer of variable size on the heap.
 */
buffer_t *buffer_alloc(size_t size);

/**
 * @brief Free buffer.
 */
void buffer_free(buffer_t *buffer);

/**
 * @brief Zero out buffer.
 */
void buffer_clear(buffer_t *buffer);

/**
 * @brief Wraps `buffer_free` for usage with cleanup attribute.
 */
static inline void buffer_cleanup(buffer_t **buffer) {
    if(*buffer != NULL) buffer_free(*buffer);
}
