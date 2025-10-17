#pragma once

#include <stdint.h>

extern uint32_t g_x86_64_fpu_area_size;
extern void (*g_x86_64_fpu_save)(void *area);
extern void (*g_x86_64_fpu_restore)(void *area);
