#pragma once

#include <stdint.h>

typedef struct {
    unsigned int width;
    unsigned int height;
    uint8_t *data;
} font_t;

extern font_t g_font_basic;
