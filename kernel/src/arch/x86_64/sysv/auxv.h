#pragma once

#include <stdint.h>

#define X86_64_AUXV_NULL 0
#define X86_64_AUXV_IGNORE 1
#define X86_64_AUXV_EXECFD 2
#define X86_64_AUXV_PHDR 3
#define X86_64_AUXV_PHENT 4
#define X86_64_AUXV_PHNUM 5
#define X86_64_AUXV_PAGESZ 6
#define X86_64_AUXV_BASE 7
#define X86_64_AUXV_FLAGS 8
#define X86_64_AUXV_ENTRY 9
#define X86_64_AUXV_NOTELF 10
#define X86_64_AUXV_UID 11
#define X86_64_AUXV_EUID 12
#define X86_64_AUXV_GID 13
#define X86_64_AUXV_EGID 14

#define X86_64_AUXV_SECURE 23

typedef struct {
    uint64_t entry;
    uint64_t phdr;
    uint64_t phent;
    uint64_t phnum;
} x86_64_auxv_t;
