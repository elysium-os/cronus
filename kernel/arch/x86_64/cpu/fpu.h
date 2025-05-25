#pragma once

#include <stdint.h>

extern uint32_t g_x86_64_fpu_area_size;
extern void (*g_x86_64_fpu_save)(void *area);
extern void (*g_x86_64_fpu_restore)(void *area);

/// Initialize FPU.
void x86_64_fpu_init();

/// Initialize FPU for current CPU.
void x86_64_fpu_init_cpu();
