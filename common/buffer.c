#include "common/buffer.h"

#include "common/assert.h"
#include "lib/mem.h"
#include "memory/heap.h"

buffer_t *buffer_alloc(size_t size) {
    ASSERT(size > 0);
    buffer_t *buffer = heap_alloc(sizeof(buffer_t) + size);
    buffer->size = size;
    return buffer;
}

void buffer_free(buffer_t *buffer) {
    ASSERT(buffer != nullptr);
    heap_free(buffer, sizeof(buffer_t) + buffer->size);
}

void buffer_clear(buffer_t *buffer) {
    mem_clear(&buffer->data, buffer->size);
}
