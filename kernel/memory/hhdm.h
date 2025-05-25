#pragma once

#include <stddef.h>
#include <stdint.h>

/// Convert a physical address to a virtual HHDM address.
/// @param ADDRESS Physical address
/// @returns Virtual address within the HHDM
#define HHDM(ADDRESS) ((uintptr_t) (ADDRESS) + g_hhdm_offset)

/// Convert a virtual address within the HHDM to a physical address.
/// @param ADDRESS Virtual address within the HHDM
/// @returns Physical address
#define HHDM_TO_PHYS(ADDRESS) ((uintptr_t) (ADDRESS) - g_hhdm_offset)

extern uintptr_t g_hhdm_offset;
extern size_t g_hhdm_size; // TODO: Maybe do some bounds checking here. maybe under a debug flag?
