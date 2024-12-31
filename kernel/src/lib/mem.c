#include "mem.h"

#include <stdint.h>

[[gnu::weak]] void memset(void *dest, int ch, size_t count) {
    for(size_t i = 0; i < count; i++) ((uint8_t *) dest)[i] = (unsigned char) ch;
}

[[gnu::weak]] void memcpy(void *dest, const void *src, size_t count) {
    for(size_t i = 0; i < count; i++) ((uint8_t *) dest)[i] = ((uint8_t *) src)[i];
}

[[gnu::weak]] void memmove(void *dest, const void *src, size_t count) {
    if(src == dest) return;
    if(src > dest) {
        for(size_t i = 0; i < count; i++) ((uint8_t *) dest)[i] = ((uint8_t *) src)[i];
    } else {
        for(size_t i = count; i > 0; i--) ((uint8_t *) dest)[i - 1] = ((uint8_t *) src)[i - 1];
    }
}

int memcmp(const void *lhs, const void *rhs, size_t count) {
    for(size_t i = 0; i < count; i++) {
        if(*((uint8_t *) lhs + i) > *((uint8_t *) rhs + i)) return -1;
        if(*((uint8_t *) lhs + i) < *((uint8_t *) rhs + i)) return 1;
    }
    return 0;
}

[[gnu::weak]] void memclear(void *dest, size_t count) {
    memset(dest, 0, count);
}
