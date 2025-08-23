#pragma once

#include <stddef.h>

#define PAGE_SIZE_4KB 0x1000
#define PAGE_SIZE_2MB 0x20'0000
#define PAGE_SIZE_1GB 0x4000'0000
#define PAGE_GRANULARITY PAGE_SIZE_4KB

#define PAGE_SIZES ((size_t[]) { PAGE_SIZE_4KB, PAGE_SIZE_2MB, PAGE_SIZE_1GB })
#define PAGE_SIZES_COUNT (sizeof(PAGE_SIZES) / sizeof(size_t))
