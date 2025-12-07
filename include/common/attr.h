#pragma once

#include "arch/cpu.h"
#include "lib/atomic.h"

#define ATTR(...) ATTR_I(__VA_ARGS__, , )
#define ATTR_I(A, B, ...) ATTR_SELECTR(A, B)

// Token-paste to a lookup macro
#define ATTR_SELECTR(A, B) ATTR_EXPAND(ATTR_##A##_##B)
#define ATTR_EXPAND(X) X

// Lookup table
#define ATTR__

#define ATTR_atomic_ ATOMIC
#define ATTR_cpu_local_ ARCH_CPU_LOCAL

#define ATTR_atomic_cpu_local ARCH_CPU_LOCAL ATOMIC
#define ATTR_cpu_local_atomic ARCH_CPU_LOCAL ATOMIC
