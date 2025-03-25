#include "draw.h"

#include "graphics/font.h"
#include "memory/hhdm.h"

static inline void putpixel(framebuffer_t *fb, uint64_t offset, draw_color_t color) {
    if(offset + sizeof(draw_color_t) > fb->size) return;
    *(draw_color_t *) (HHDM(fb->physical_address) + offset) = color;
}

draw_color_t draw_color(uint8_t r, uint8_t g, uint8_t b) {
    return (r << 16) | (g << 8) | (b << 0);
}

void draw_pixel(framebuffer_t *fb, unsigned int x, unsigned int y, draw_color_t color) {
    if(x >= fb->width || y >= fb->height) return;
    putpixel(fb, (y * fb->pitch) + (x * sizeof(draw_color_t)), color);
}

void draw_char(framebuffer_t *fb, unsigned int x, unsigned int y, char c, font_t *font, draw_color_t color) {
    int w = font->width;
    int h = font->height;

    uint8_t *font_char = &font->data[((unsigned int) (uint8_t) c) * font->width * font->height / 8];

    uint64_t offset = (x * sizeof(draw_color_t)) + (y * fb->pitch);
    for(int yy = 0; yy < h && y + yy < fb->height; yy++) {
        for(int xx = 0; xx < w && x + xx < fb->width; xx++) {
            if(font_char[yy] & (1 << (w - xx))) putpixel(fb, offset + xx * sizeof(draw_color_t), color);
        }
        offset += fb->pitch;
    }
}

void draw_rect(framebuffer_t *fb, unsigned int x, unsigned int y, uint16_t w, uint16_t h, draw_color_t color) {
    if(x + w > fb->width) w = fb->width - x;
    if(y + h > fb->height) h = fb->height - y;
    uint64_t offset = (y * fb->pitch) + (x * sizeof(draw_color_t));
    for(int yy = 0; yy < h; yy++) {
        uint64_t local_offset = offset;
        for(int xx = 0; xx < w; xx++) {
            putpixel(fb, local_offset, color);
            local_offset += sizeof(draw_color_t);
        }
        offset += fb->pitch;
    }
}
