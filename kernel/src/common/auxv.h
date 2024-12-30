#pragma once

#include <stdint.h>

#define AUXV_NULL 0
#define AUXV_IGNORE 1
#define AUXV_EXECFD 2
#define AUXV_PHDR 3
#define AUXV_PHENT 4
#define AUXV_PHNUM 5
#define AUXV_PAGESZ 6
#define AUXV_BASE 7
#define AUXV_FLAGS 8
#define AUXV_ENTRY 9
#define AUXV_NOTELF 10
#define AUXV_UID 11
#define AUXV_EUID 12
#define AUXV_GID 13
#define AUXV_EGID 14

#define AUXV_SECURE 23

typedef struct {
    uint64_t entry;
    uint64_t phdr;
    uint64_t phent;
    uint64_t phnum;
} auxv_t;
