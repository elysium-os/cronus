#pragma once

#include <stddef.h>

#define X86_64_INIT_FLAG_MEMORY_PHYS (1 << 0)
#define X86_64_INIT_FLAG_MEMORY_PHYS_EARLY (1 << 1)
#define X86_64_INIT_FLAG_MEMORY_VIRT (1 << 2)
#define X86_64_INIT_FLAG_INTERRUPTS (1 << 3)
#define X86_64_INIT_FLAG_SMP (1 << 4)
#define X86_64_INIT_FLAG_TIME (1 << 5)
#define X86_64_INIT_FLAG_SCHED (1 << 6)
#define X86_64_INIT_FLAG_VFS (1 << 7)

/// Check if given initialization flags are set.
bool x86_64_init_flag_check(size_t flags);

/// Set an initialization flag.
void x86_64_init_flag_set(size_t flags);
