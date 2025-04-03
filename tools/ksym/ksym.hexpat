struct String {
    char value[];
};

struct KSymHeader {
    u64 names_offset;
    u64 names_size;
    u64 symbols_offset;
    u64 symbol_count;
};

fn symbol_format(auto s) {
    String name @ header.names_offset + s.name_index;
    return name.value;
};

struct KSymSymbol {
    u64 name_index;
    u64 value;
} [[format("symbol_format")]];

KSymHeader header @ 0x0;
KSymSymbol symbols[header.symbol_count] @ header.symbols_offset;