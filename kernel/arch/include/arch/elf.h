#pragma once

#include "abi/sysv/elf64.h"

#include <stdint.h>

/// Perform relocation for given rela entry.
bool elf_do_relocation_addend(elf64_rela_t *rela, elf64_symbol_t *symbol, uintptr_t section_address);
