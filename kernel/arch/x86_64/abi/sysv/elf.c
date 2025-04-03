#include "arch/elf.h"

#include "common/abi/sysv/elf64.h"
#include "common/log.h"
#include "lib/mem.h"

#include <stddef.h>
#include <stdint.h>

#define R_X86_64_NONE 0
#define R_X86_64_64 1
#define R_X86_64_PC32 2
#define R_X86_64_PLT32 4
#define R_X86_64_32 10
#define R_X86_64_32S 11
#define R_X86_64_PC64 24

bool arch_elf_do_relocation_addend(elf64_rela_t *rela, elf64_symbol_t *symbol, uintptr_t section_address) {
    void *address = (void *) (section_address + rela->offset);

    LOG_DEVELOPMENT("ELF", "relocation: [addr: %#lx] [sym_val: %#lx] [addend: %li]", address, symbol->value, rela->addend);

    uint64_t value = symbol->value + rela->addend;

    size_t reloc_size;
    switch(ELF64_R_TYPE(rela->info)) {
        case R_X86_64_NONE: return true;
        case R_X86_64_64:   reloc_size = 8; break;
        case R_X86_64_32:
            if(value != *(uint32_t *) &value) goto overflow;
            reloc_size = 4;
            break;
        case R_X86_64_32S:
            if((int64_t) value != *(int32_t *) &value) goto overflow;
            reloc_size = 4;
            break;
        case R_X86_64_PLT32:
        case R_X86_64_PC32:
            value -= (uint64_t) address;
            reloc_size = 4;
            break;
        case R_X86_64_PC64:
            value -= (uint64_t) address;
            reloc_size = 8;
            break;
        default: log(LOG_LEVEL_ERROR, "ELF", "unsupported relocation type: %lu", ELF64_R_TYPE(rela->info)); return false;
    }

    LOG_DEVELOPMENT("ELF", "relocation: %#lx -> %#lx (%lu)", symbol->value + rela->addend, value, reloc_size);

    size_t zero = 0LL;
    if(memcmp(address, &zero, reloc_size) != 0) {
        log(LOG_LEVEL_ERROR, "ELF", "relocation %lu address is nonzero", ELF64_R_TYPE(rela->info));
        return false;
    }

    memcpy(address, &value, reloc_size);
    return true;

overflow:
    log(LOG_LEVEL_ERROR, "ELF", "relocation %lu overflows", ELF64_R_TYPE(rela->info));
    return false;
}
