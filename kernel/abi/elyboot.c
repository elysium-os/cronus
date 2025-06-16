#include "memory/page.h"

#include <stdint.h>

typedef struct [[gnu::packed]] {
    uint64_t page_struct_size;
} kernel_data_t;

[[gnu::section(".elyboot"), gnu::used]] static volatile kernel_data_t g_kernel_data = { .page_struct_size = sizeof(page_t) };
