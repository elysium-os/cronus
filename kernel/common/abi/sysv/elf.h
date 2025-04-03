#pragma once

#include "fs/vfs.h"
#include "lib/param.h"
#include "memory/vm.h"

#include <stdint.h>

typedef enum {
    ELF_RESULT_OK,
    ELF_RESULT_ERR_FS,
    ELF_RESULT_ERR_NOT_ELF,
    ELF_RESULT_ERR_MALFORMED,
    ELF_RESULT_ERR_UNSUPPORTED,
    ELF_RESULT_ERR_INVALID_TYPE,
    ELF_RESULT_ERR_INVALID_CLASS,
    ELF_RESULT_ERR_INVALID_ENDIAN,
    ELF_RESULT_ERR_INVALID_MACHINE,
    ELF_RESULT_ERR_NOT_FOUND,
} elf_result_t;

typedef struct elf_file {
    vfs_node_t *file;
    uintptr_t entry;
    struct {
        size_t entry_size;
        size_t offset, count;
    } program_headers;
} elf_file_t;

/**
 * @brief Read and verify elf file.
 */
elf_result_t elf_read(vfs_node_t *file, PARAM_OUT(elf_file_t **) elf_file);

/**
 * @brief Lookup interpreter from ELF file.
 */
elf_result_t elf_lookup_interpreter(elf_file_t *elf_file, PARAM_OUT(char **) interpreter);

/**
 * @brief Lookup program header table virtual address from ELF file.
 */
elf_result_t elf_lookup_phdr_address(elf_file_t *elf_file, PARAM_OUT(uintptr_t *) phdr_address);

/**
 * @brief Load ELF file into address space.
 * @todo Error here does not discard the allocated space.
 */
elf_result_t elf_load(elf_file_t *elf_file, vm_address_space_t *as);
