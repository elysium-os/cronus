#pragma once

#include <stddef.h>

#define X86_64_INIT_FLAG_MEMORY_PHYS (1 << 0)
#define X86_64_INIT_FLAG_MEMORY_VIRT (1 << 1)
#define X86_64_INIT_FLAG_INTERRUPTS (1 << 2)
#define X86_64_INIT_FLAG_SMP (1 << 3)
#define X86_64_INIT_FLAG_SCHED (1 << 4)

bool x86_64_init_flag_check(size_t flag);
void x86_64_init_flag_set(size_t flag);
