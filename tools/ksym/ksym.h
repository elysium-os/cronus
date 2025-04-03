#pragma once

#include <stdint.h>

#define KSYM_IDENTIFIER1 'K'
#define KSYM_IDENTIFIER2 'S'
#define KSYM_IDENTIFIER3 'y'
#define KSYM_IDENTIFIER4 'M'

typedef struct {
    uint64_t name_index;
    uint64_t size;
    uint64_t value;
} __attribute__((packed)) ksym_symbol_t;

typedef struct {
    char identifier[4];
    uint64_t names_offset, names_size;
    uint64_t symbols_offset, symbols_count;
} __attribute__((packed)) ksym_header_t;
