#include "kernel_symbol.h"

#include "common/log.h"
#include "common/panic.h"
#include "lib/mem.h"
#include "lib/string.h"

#include <stddef.h>
#include <stdint.h>

#define IDENTIFIER "KSyM"
#define REVISION 2

#define IS_GLOBAL(KSYM) (((KSYM)->flags & 1) != 0)

#define SYMBOL_AT(HEADER, INDEX) ((symbol_t *) ((uintptr_t) (HEADER) + (HEADER)->symbols_offset + ((HEADER)->symbol_size * (INDEX))))

typedef struct [[gnu::packed]] {
    uint8_t identifier[4];
    uint8_t revision;
    uint8_t rsv0[3];
    uint64_t names_offset, names_length;
    uint64_t symbols_offset, symbol_size, symbols_count;
} header_t;

typedef struct [[gnu::packed]] {
    uint64_t name_offset;
    uint16_t flags;
    uint16_t rsv0;
    uint32_t rsv1;
    uint64_t size;
    uint64_t value;
} symbol_t;

static char g_invalid_name[1] = { '\0' };
static header_t *g_kernel_symbols_header = nullptr;

static const char *get_name(header_t *header, uint64_t name_offset) {
    if(name_offset >= header->names_length) {
        log(LOG_LEVEL_ERROR, "KERNEL_SYMBOL", "name lookup offset exceeds buffer length");
        return g_invalid_name;
    }
    return (char *) ((uintptr_t) header + header->names_offset + name_offset);
}

void kernel_symbols_load(void *symbol_data) {
    if(g_kernel_symbols_header != nullptr) log(LOG_LEVEL_WARN, "KERNEL_SYMBOL", "symbol data reloaded");
    if(memcmp(symbol_data, IDENTIFIER, 4) != 0) panic("KERNEL_SYMBOL", "invalid kernel symbol file identifier");
    header_t *header = (header_t *) symbol_data;
    if(header->revision > REVISION) panic("KERNEL_SYMBOL", "invalid kernel symbol file revision");
    g_kernel_symbols_header = header;
}

bool kernel_symbols_is_loaded() {
    return g_kernel_symbols_header != nullptr;
}

bool kernel_symbol_lookup_by_address(uintptr_t address, PARAM_FILL(kernel_symbol_t *) symbol) {
    if(!kernel_symbols_is_loaded()) return false;

    symbol_t *prev = nullptr, *sym = nullptr;
    for(size_t i = 0; i < g_kernel_symbols_header->symbols_count; i++) {
        symbol_t *ksymbol = SYMBOL_AT(g_kernel_symbols_header, i);
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
    if(sym == nullptr) return false;

    symbol->name = get_name(g_kernel_symbols_header, sym->name_offset);
    symbol->address = sym->value;
    symbol->size = sym->size;
    symbol->global = IS_GLOBAL(sym);
    return true;
}

bool kernel_symbol_lookup_by_name(const char *name, PARAM_FILL(kernel_symbol_t *) symbol) {
    if(!kernel_symbols_is_loaded()) return false;

    for(size_t i = 0; i < g_kernel_symbols_header->symbols_count; i++) {
        symbol_t *ksymbol = SYMBOL_AT(g_kernel_symbols_header, i);
        if(!IS_GLOBAL(ksymbol)) continue;
        if(!string_eq(get_name(g_kernel_symbols_header, ksymbol->name_offset), name)) continue;

        symbol->name = get_name(g_kernel_symbols_header, ksymbol->name_offset);
        symbol->address = ksymbol->value;
        symbol->size = ksymbol->size;
        symbol->global = IS_GLOBAL(ksymbol);
        return true;
    }

    return false;
}
