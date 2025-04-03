#include "kernel_symbol.h"

#include "common/log.h"
#include "common/panic.h"
#include "ksym.h"
#include "lib/mem.h"
#include "lib/string.h"

#include <stddef.h>
#include <stdint.h>

#define SYMBOL_AT(HEADER, INDEX) ((ksym_symbol_t *) ((uintptr_t) (HEADER) + (HEADER)->symbols_offset + (sizeof(ksym_symbol_t) * (INDEX))))
#define SYMBOL_NAME(HEADER, NAME_INDEX) ((char *) ((uintptr_t) (HEADER) + (HEADER)->names_offset + (NAME_INDEX)))

static ksym_header_t *g_kernel_symbols_header = NULL;

void kernel_symbols_load(void *ksym_data) {
    if(kernel_symbols_is_loaded()) panic("ksym data already loaded");
    if(memcmp(ksym_data, &(char[]) { KSYM_IDENTIFIER1, KSYM_IDENTIFIER2, KSYM_IDENTIFIER3, KSYM_IDENTIFIER4 }, 4) != 0) panic("invalid ksym data");
    g_kernel_symbols_header = (ksym_header_t *) ksym_data;
}

bool kernel_symbols_is_loaded() {
    return g_kernel_symbols_header != NULL;
}

bool kernel_symbol_lookup(uintptr_t address, PARAM_FILL(kernel_symbol_t *) symbol) {
    if(!kernel_symbols_is_loaded()) return false;

    ksym_symbol_t *prev = NULL, *sym = NULL;
    for(size_t i = 0; i < g_kernel_symbols_header->symbols_count; i++) {
        ksym_symbol_t *ksymbol = SYMBOL_AT(g_kernel_symbols_header, i);
        if(ksymbol->value > address) {
            sym = prev;
            break;
        }
        if(address >= ksymbol->value && address < ksymbol->value + ksymbol->size) {
            sym = ksymbol;
            break;
        }

        prev = ksymbol;
    }
    if(sym == NULL) return false;

    symbol->name = SYMBOL_NAME(g_kernel_symbols_header, sym->name_index);
    symbol->address = sym->value;
    symbol->size = sym->size;
    return true;
}

bool kernel_symbol_lookup_by_name(const char *name, PARAM_FILL(kernel_symbol_t *) symbol) {
    if(!kernel_symbols_is_loaded()) return false;

    for(size_t i = 0; i < g_kernel_symbols_header->symbols_count; i++) {
        ksym_symbol_t *ksymbol = SYMBOL_AT(g_kernel_symbols_header, i);
        if(!string_eq(SYMBOL_NAME(g_kernel_symbols_header, ksymbol->name_index), name)) continue;

        symbol->name = SYMBOL_NAME(g_kernel_symbols_header, ksymbol->name_index);
        symbol->address = ksymbol->value;
        symbol->size = ksymbol->size;
        return true;
    }

    return false;
}
