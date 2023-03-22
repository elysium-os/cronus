#include "keyboard.h"
#include <cpu/irq.h>

static int8_t g_scancodes[128];
static keyboard_handler_t g_keyboard_handler;

static uint8_t g_layout_us[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t',   // <-- Tab
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,      // <-- control key
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',  0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*',
    0,      // Alt
    ' ',    // Space bar
    0,      // Caps lock
    0,      // 59 - F1 key ...>
    0, 0, 0, 0, 0, 0, 0, 0,
    0,      // <... F10
    0,      // 69 - Num lock
    0,      // Scroll Lock
    0,      // Home key
    0,      // Up Arrow
    0,      // Page Up
    '-',
    0,      // Left Arrow
    0,
    0,      // Right Arrow
    '+',
    0,      // 79 - End key
    0,      // Down Arrow
    0,      // Page Down
    0,      // Insert Key
    0,      // Delete Key
    0, 0, 0,
    0,      // F11 Key
    0,      // F12 Key
    0,      // All other keys are undefined
};

static void keyboard_event(irq_frame_t *registers __attribute__((unused))) {
    uint8_t scancode = ps2_port_read(false);

    if(scancode >= 0x80) {
        g_scancodes[scancode - 0x80] = 0;
        return;
    }
    g_scancodes[scancode] = 1;

    uint8_t character = g_layout_us[scancode];
    if(character >= 'a' && character <= 'z' && (g_scancodes[0x36] || g_scancodes[0x2a])) character = g_layout_us[scancode] - 32;
    if(g_keyboard_handler) g_keyboard_handler(character);
}

void keyboard_initialize(ps2_ports_t port) {
    irq_register_handler(32 + 6 + port, keyboard_event);
}

void keyboard_set_handler(keyboard_handler_t handler) {
    g_keyboard_handler = handler;
}