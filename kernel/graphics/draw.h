#pragma once

#include "graphics/font.h"
#include "graphics/framebuffer.h"

#include <stdint.h>

typedef uint32_t draw_color_t;

/// Pack RGB values into a color value.
draw_color_t draw_color(uint8_t r, uint8_t g, uint8_t b);

/// Draw a single pixel to a context.
void draw_pixel(framebuffer_t *fb, unsigned int x, unsigned int y, draw_color_t color);

/// Draw a character to a context.
void draw_char(framebuffer_t *fb, unsigned int x, unsigned int y, char c, font_t *font, draw_color_t color);

/// Draw a rectangle to a context.
/// @param w rectangle width
/// @param h rectangle height
void draw_rect(framebuffer_t *fb, unsigned int x, unsigned int y, uint16_t w, uint16_t h, draw_color_t color);
