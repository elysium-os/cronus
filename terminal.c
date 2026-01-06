#include "common/log.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "graphics/framebuffer.h"
#include "lib/format.h"
#include "sys/init.h"

#include <stdarg.h>

static unsigned int g_x = 0, g_y = 0;
static draw_color_t g_color;

static void write_char(char ch) {
    unsigned int initial_y = g_y;
    switch(ch) {
        case '\n':
            g_x = 0;
            g_y += g_font_basic.height;
            break;
        default:
            draw_char(&g_framebuffer, g_x, g_y, ch, &g_font_basic, g_color);
            g_x += g_font_basic.width;
            break;
    }

    if(g_x >= g_framebuffer.width) {
        g_x = 0;
        g_y += g_font_basic.height;
    }
    if(g_y >= g_framebuffer.height) g_y = 0;

    if(initial_y != g_y) draw_rect(&g_framebuffer, 0, g_y, g_framebuffer.width, g_font_basic.height, draw_color(14, 14, 15));
}

static void serial_format(char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    format(write_char, fmt, list);
    va_end(list);
}

static void log_fb(log_level_t level, const char *tag, const char *fmt, va_list args) {
    switch(level) {
        case LOG_LEVEL_INFO:  g_color = draw_color(237, 150, 0); break;
        case LOG_LEVEL_WARN:  g_color = draw_color(255, 94, 94); break;
        case LOG_LEVEL_ERROR: g_color = draw_color(255, 15, 15); break;
        default:              g_color = draw_color(237, 150, 0); break;
    }
    switch(level) {
        case LOG_LEVEL_TRACE: g_color = draw_color(87, 89, 89); break; /* Gray */
        case LOG_LEVEL_DEBUG: g_color = draw_color(45, 139, 173); break; /* Cyan */
        case LOG_LEVEL_INFO:  g_color = draw_color(56, 224, 73); break; /* Green */
        case LOG_LEVEL_WARN:  g_color = draw_color(240, 232, 79); break; /* Yellow */
        case LOG_LEVEL_ERROR: g_color = draw_color(240, 106, 79); break; /* Light Red  */
        case LOG_LEVEL_FATAL: g_color = draw_color(196, 33, 0); break; /* Red */
        default:              g_color = draw_color(255, 255, 255); break;
    }
    serial_format("[%s:%s] ", log_level_stringify(level), tag, "\e[0m");

    g_color = draw_color(255, 255, 255);
    format(write_char, fmt, args);

    write_char('\n');
}

static log_sink_t g_terminal_sink = {
    .name = "FB_TERM",
    .filter = { .level = LOG_LEVEL_INFO, .tags_as_include = false, .tags = nullptr, .tag_count = 0 },
    .log = log_fb
};

INIT_TARGET(terminal, INIT_STAGE_EARLY, INIT_SCOPE_BSP, INIT_DEPS()) {
    draw_rect(&g_framebuffer, 0, 0, g_framebuffer.width, g_framebuffer.height, draw_color(14, 14, 15));
    log_sink_add(&g_terminal_sink);
}
