#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uintptr_t physical_address;
    size_t size;

    uint64_t height;
    uint64_t width;
    uint64_t pitch;
} framebuffer_t;

extern framebuffer_t g_framebuffer;