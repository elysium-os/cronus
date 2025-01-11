#pragma once

#include "common/log.h"

extern log_sink_t g_x86_64_qemu_serial_sink;

/**
 * @brief Print a character to qemu debug.
 */
void x86_64_qemu_serial_putc(char ch);