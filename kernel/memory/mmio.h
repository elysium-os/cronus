#pragma once

#include "arch/mmio.h"

#include <stdint.h>

/// Map a region of memory for MMIO.
void *mmio_map(uintptr_t address, uintptr_t length);

/// Unmap an MMIO region.
void mmio_unmap(void *address, uintptr_t length);
