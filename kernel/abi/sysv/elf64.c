#include "elf64.h"

unsigned long elf64_hash(const unsigned char *name) {
    unsigned long h = 0, g;
    while(*name) {
        h = (h << 4) + *name++;
        if((g = (h & 0xF0000000))) h ^= (g >> 24);
        h &= 0x0FFFFFFF;
    }
    return h;
}
