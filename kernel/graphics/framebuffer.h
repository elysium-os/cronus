#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    void *address;
    size_t size;
    uint64_t height, width, pitch;
} framebuffer_t;

extern framebuffer_t g_framebuffer;
