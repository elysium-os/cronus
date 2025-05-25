#pragma once

#include "lib/param.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;
    bool global;
    size_t size;
    uintptr_t address;
} kernel_symbol_t;

/// Load and validate symbol file as kernel symbols.
void kernel_symbols_load(void *symbol_data);

/// Are kernel symbols loaded.
bool kernel_symbols_is_loaded();

/// Lookup kernel symbol by address.
/// @returns true on success
bool kernel_symbol_lookup_by_address(uintptr_t address, PARAM_FILL(kernel_symbol_t *) symbol);

/// Lookyp kernel symbol by name. This will only return global symbols.
/// @returns true on success
bool kernel_symbol_lookup_by_name(const char *name, PARAM_FILL(kernel_symbol_t *) symbol);
