#pragma once

#include "fs/vfs.h"
#include "lib/param.h"
#include "memory/vm.h"

#include "arch/x86_64/abi/sysv/auxv.h"

typedef enum {
    ELF_RESULT_OK,
    ELF_RESULT_ERR_FS,
    ELF_RESULT_ERR_NOT_ELF,
    ELF_RESULT_ERR_UNSUPPORTED,
    ELF_RESULT_ERR_INVALID_PHDR,
    ELF_RESULT_ERR_NOT_64BIT,
    ELF_RESULT_ERR_NOT_LITTLE_ENDIAN,
    ELF_RESULT_ERR_NOT_X86_64,
} elf_result_t;

/**
 * @brief Load ELF into address space.
 * @warn on fail mapped program segments may not be unmapped
 */
elf_result_t elf_load(vfs_node_t *file, vm_address_space_t *as, PARAM_OUT(char **) interpreter, PARAM_OUT(x86_64_auxv_t *) auxv);
