#pragma once

#include <stddef.h>

#ifdef __ARCH_X86_64

#define ARCH_PAGE_SIZE_4KB 0x1000
#define ARCH_PAGE_SIZE_2MB 0x20'0000
#define ARCH_PAGE_SIZE_1GB 0x4000'0000
#define ARCH_PAGE_GRANULARITY ARCH_PAGE_SIZE_4KB

#define ARCH_PAGE_SIZES ((size_t[]) {ARCH_PAGE_SIZE_4KB, ARCH_PAGE_SIZE_2MB, ARCH_PAGE_SIZE_1GB})
#define ARCH_PAGE_SIZES_COUNT (sizeof(ARCH_PAGE_SIZES) / sizeof(size_t))

#else
#error Unimplemented
#endif

static_assert(ARCH_PAGE_GRANULARITY > 0);
static_assert(ARCH_PAGE_SIZES_COUNT > 0);