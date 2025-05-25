#pragma once

#include "common/log.h"

extern log_sink_t g_x86_64_qemu_debug_sink;

/// Print a character to qemu debug.
void x86_64_qemu_debug_putc(char ch);
