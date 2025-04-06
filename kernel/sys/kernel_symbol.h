#pragma once

#include "lib/param.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;
    size_t size;
    uintptr_t address;
} kernel_symbol_t;

/**
 * @brief Load and validate symbol file as kernel symbols.
 */
void kernel_symbols_load(void *symbol_data);

/**
 * @brief Are kernel symbols loaded.
 */
bool kernel_symbols_is_loaded();

/**
 * @brief Lookup kernel symbol by address.
 * @returns true on success
 */
bool kernel_symbol_lookup(uintptr_t address, PARAM_FILL(kernel_symbol_t *) symbol);

/**
 * @brief Lookyp kernel symbol by name.
 * @returns true on success
 */
bool kernel_symbol_lookup_by_name(const char *name, PARAM_FILL(kernel_symbol_t *) symbol);
