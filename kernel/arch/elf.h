#pragma once

#include "abi/sysv/elf64.h"

#include <stdint.h>

#ifdef __ARCH_X86_64

#define ARCH_ELF_CLASS ELF64_CLASS64
#define ARCH_ELF_ENCODING ELF64_DATA2LSB
#define ARCH_ELF_MACHINE 0x3E

#endif

#if !defined(ARCH_ELF_CLASS) || !defined(ARCH_ELF_ENCODING) || !defined(ARCH_ELF_MACHINE)
#error Missing implementation
#endif

/**
 * @brief Perform relocation for given rela entry.
 */
bool arch_elf_do_relocation_addend(elf64_rela_t *rela, elf64_symbol_t *symbol, uintptr_t section_address);
