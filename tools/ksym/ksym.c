#include "ksym.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char **argv) {
    if(argc < 2) {
        fprintf(stderr, "missing symbols file");
        return EXIT_FAILURE;
    }

    if(argc < 3) {
        fprintf(stderr, "missing output file");
        return EXIT_FAILURE;
    }

    bool verbose = false;
    if(argc >= 4 && strcmp(argv[3], "verbose") == 0) verbose = true;

    FILE *symbols_file = fopen(argv[1], "r");
    if(symbols_file == NULL) {
        fprintf(stderr, "failed to open symbols file `%s`", strerror(errno));
        return EXIT_FAILURE;
    }

    int symbols_fd = fileno(symbols_file);
    if(symbols_fd < 0) {
        fprintf(stderr, "failed to get symbols fd `%s`", strerror(errno));
        return EXIT_FAILURE;
    }

    struct stat stat;
    if(fstat(symbols_fd, &stat) != 0) {
        fprintf(stderr, "failed to stat symbols file `%s`", strerror(errno));
        return EXIT_FAILURE;
    }

    char *names = NULL;
    size_t names_size = 0;

    ksym_symbol_t *symbols = NULL;
    size_t symbol_count = 0;

    char *symbols_data = malloc(stat.st_size);
    if(fread(symbols_data, 1, stat.st_size, symbols_file) != stat.st_size) {
        fprintf(stderr, "failed to read symbols file `%s`", strerror(errno));
        return EXIT_FAILURE;
    }

    for(size_t i = 0; i < stat.st_size;) {
        assert(stat.st_size - i >= 36);

        // parse address
        char address_str[17] = {};
        memcpy(&address_str, &symbols_data[i], 16);
        uintptr_t value = strtoull(address_str, NULL, 16);

        i += 16;

        i += 1; // space

        // parse size
        char size_str[17] = {};
        memcpy(&size_str, &symbols_data[i], 16);
        uintptr_t size = strtoull(address_str, NULL, 16);

        i += 16;

        i += 1; // space

        // parse type
        i += 1;

        i += 1; // space

        // parse & write name
        size_t name_length = 0;
        for(size_t j = i; j < stat.st_size && symbols_data[j] != '\n'; j++) name_length++;

        size_t name_index = names_size;
        names_size += name_length + 1;
        names = realloc(names, names_size);
        memcpy(&names[name_index], &symbols_data[i], name_length);
        names[name_index + name_length] = '\0';

        i += name_length + 1;

        // write symbol
        symbols = realloc(symbols, sizeof(ksym_symbol_t) * (symbol_count + 1));
        symbols[symbol_count] = (ksym_symbol_t) { .name_index = name_index, .size = size, .value = value };
        symbol_count += 1;

        // print symbol
        if(verbose) printf("%#lx | %#lx | %s\n", value, size, &names[name_index]);
    }

    fclose(symbols_file);

    FILE *out_file = fopen(argv[2], "w");
    if(out_file == NULL) {
        fprintf(stderr, "failed to open symbols file `%s`", strerror(errno));
        return EXIT_FAILURE;
    }

    if(fwrite(
           &(ksym_header_t) { .identifier[0] = KSYM_IDENTIFIER1,
                              .identifier[1] = KSYM_IDENTIFIER2,
                              .identifier[2] = KSYM_IDENTIFIER3,
                              .identifier[3] = KSYM_IDENTIFIER4,
                              .names_offset = sizeof(ksym_header_t),
                              .names_size = names_size,
                              .symbols_offset = sizeof(ksym_header_t) + names_size,
                              .symbols_count = symbol_count },
           sizeof(ksym_header_t),
           1,
           out_file
       ) != 1)
    {
        fprintf(stderr, "failed to write header to output `%s`", strerror(errno));
        return EXIT_FAILURE;
    }

    if(fwrite(names, 1, names_size, out_file) != names_size) {
        fprintf(stderr, "failed to names to output `%s`", strerror(errno));
        return EXIT_FAILURE;
    }

    if(fwrite(symbols, sizeof(ksym_symbol_t), symbol_count, out_file) != symbol_count) {
        fprintf(stderr, "failed to symbols to output `%s`", strerror(errno));
        return EXIT_FAILURE;
    }

    fclose(out_file);

    if(names != NULL) free(names);
    if(symbols != NULL) free(symbols);
    free(symbols_data);

    return 0;
}
