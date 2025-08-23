#include "abi/sysv/elf.h"

#include "abi/sysv/elf64.h"
#include "arch/elf.h"
#include "arch/page.h"
#include "common/assert.h"
#include "common/log.h"
#include "fs/vfs.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "lib/param.h"
#include "memory/heap.h"

#include <stddef.h>
#include <stdint.h>

static bool read_phdr(elf_file_t *elf_file, size_t index, PARAM_FILL(elf64_program_header_t *) phdr) {
    size_t read_count;
    vfs_result_t res =
        elf_file->file->ops->rw(elf_file->file, &(vfs_rw_t) { .rw = VFS_RW_READ, .size = sizeof(elf64_program_header_t), .offset = elf_file->program_headers.offset + (index * elf_file->program_headers.entry_size), .buffer = phdr }, &read_count);
    return res != VFS_RESULT_OK || read_count != sizeof(elf64_program_header_t);
}

static void auto_free_header(elf64_file_header_t **header) {
    heap_free(*header, sizeof(header));
}

static void auto_free_phdr(elf64_program_header_t **phdr) {
    heap_free(*phdr, sizeof(phdr));
}

elf_result_t elf_read(vfs_node_t *file, PARAM_OUT(elf_file_t **) elf_file) {
    if(file->type != VFS_NODE_TYPE_FILE) return ELF_RESULT_ERR_NOT_ELF;

    vfs_node_attr_t attr;
    vfs_result_t res = file->ops->attr(file, &attr);
    if(res != VFS_RESULT_OK) return ELF_RESULT_ERR_FS;
    if(attr.size < sizeof(elf64_file_header_t)) return ELF_RESULT_ERR_NOT_ELF;

    [[gnu::cleanup(auto_free_header)]] elf64_file_header_t *header = heap_alloc(sizeof(elf64_file_header_t));

    size_t read_count;
    res = file->ops->rw(file, &(vfs_rw_t) { .rw = VFS_RW_READ, .size = sizeof(elf64_file_header_t), .buffer = (void *) header }, &read_count);
    if(res != VFS_RESULT_OK || read_count != sizeof(elf64_file_header_t)) return ELF_RESULT_ERR_FS;

    if(!ELF64_ID_VALIDATE(header->ident.magic)) return ELF_RESULT_ERR_NOT_ELF;
    if(header->ident.class != ARCH_ELF_CLASS) return ELF_RESULT_ERR_INVALID_CLASS;
    if(header->ident.encoding != ARCH_ELF_ENCODING) return ELF_RESULT_ERR_INVALID_ENDIAN;
    if(header->version > 1) return ELF_RESULT_ERR_UNSUPPORTED;
    if(header->type != ELF64_ET_EXEC) return ELF_RESULT_ERR_INVALID_TYPE;
    if(header->machine != ARCH_ELF_MACHINE) return ELF_RESULT_ERR_INVALID_MACHINE;
    if(header->phnum == 0 || header->phentsize < sizeof(elf64_program_header_t)) return ELF_RESULT_ERR_UNSUPPORTED;

    *elf_file = heap_alloc(sizeof(elf_file_t));
    **elf_file = (elf_file_t) {
        .file = file,
        .entry = header->entry,
        .program_headers = { .offset = header->phoff, .count = header->phnum, .entry_size = header->phentsize }
    };
    return ELF_RESULT_OK;
}

elf_result_t elf_lookup_interpreter(elf_file_t *elf_file, PARAM_OUT(char **) interpreter) {
    [[gnu::cleanup(auto_free_phdr)]] elf64_program_header_t *phdr = heap_alloc(elf_file->program_headers.entry_size);
    for(size_t i = 0; i < elf_file->program_headers.count; i++) {
        if(read_phdr(elf_file, i, phdr)) return ELF_RESULT_ERR_FS;

        if(phdr->type != ELF64_PT_INTERP) continue;

        size_t interpreter_size = phdr->filesz + 1;
        char *interp = heap_alloc(interpreter_size);
        memclear(interp, interpreter_size);

        size_t read_count;
        vfs_result_t res = elf_file->file->ops->rw(elf_file->file, &(vfs_rw_t) { .rw = VFS_RW_READ, .buffer = interp, .offset = phdr->offset, .size = phdr->filesz }, &read_count);
        if(res != VFS_RESULT_OK || read_count != phdr->filesz) {
            heap_free(interp, interpreter_size);
            return ELF_RESULT_ERR_FS;
        }

        *interpreter = interp;
        return ELF_RESULT_OK;
    }
    return ELF_RESULT_ERR_NOT_FOUND;
}

elf_result_t elf_lookup_phdr_address(elf_file_t *elf_file, PARAM_OUT(uintptr_t *) phdr_address) {
    [[gnu::cleanup(auto_free_phdr)]] elf64_program_header_t *phdr = heap_alloc(elf_file->program_headers.entry_size);
    for(size_t i = 0; i < elf_file->program_headers.count; i++) {
        if(read_phdr(elf_file, i, phdr)) return ELF_RESULT_ERR_FS;

        if(phdr->type != ELF64_PT_PHDR) continue;

        *phdr_address = phdr->vaddr;
        return ELF_RESULT_OK;
    }
    return ELF_RESULT_ERR_NOT_FOUND;
}

elf_result_t elf_load(elf_file_t *elf_file, vm_address_space_t *as) {
    [[gnu::cleanup(auto_free_phdr)]] elf64_program_header_t *phdr = heap_alloc(elf_file->program_headers.entry_size);
    for(size_t i = 0; i < elf_file->program_headers.count; i++) {
        if(read_phdr(elf_file, i, phdr)) return ELF_RESULT_ERR_FS;

        switch(phdr->type) {
            case ELF64_PT_LOAD:
                if(phdr->filesz > phdr->memsz) return ELF_RESULT_ERR_MALFORMED;

                vm_protection_t prot = { .read = true };
                if((phdr->flags & ELF64_PF_R) != 0) log(LOG_LEVEL_WARN, "ELF", "non-read permission program headers not supported");
                if((phdr->flags & ELF64_PF_W) != 0) prot.write = true;
                if((phdr->flags & ELF64_PF_X) != 0) prot.exec = true;

                uintptr_t aligned_vaddr = MATH_FLOOR(phdr->vaddr, PAGE_GRANULARITY);
                size_t length = MATH_CEIL(phdr->memsz + (phdr->vaddr - aligned_vaddr), PAGE_GRANULARITY);

                void *ptr = vm_map_anon(as, (void *) aligned_vaddr, length, prot, VM_CACHE_STANDARD, VM_FLAG_FIXED | VM_FLAG_ZERO | VM_FLAG_DYNAMICALLY_BACKED);
                ASSERT(ptr != nullptr);
                if(phdr->filesz > 0) {
                    size_t buffer_size = MATH_CEIL(phdr->filesz, PAGE_GRANULARITY);
                    void *buffer = vm_map_anon(g_vm_global_address_space, nullptr, buffer_size, VM_PROT_RW, VM_CACHE_STANDARD, VM_FLAG_NONE);
                    ASSERT(buffer != nullptr);

                    size_t read_count;
                    vfs_result_t res = elf_file->file->ops->rw(elf_file->file, &(vfs_rw_t) { .rw = VFS_RW_READ, .size = phdr->filesz, .offset = phdr->offset, .buffer = buffer }, &read_count);
                    if(res != VFS_RESULT_OK || read_count != phdr->filesz) {
                        vm_unmap(g_vm_global_address_space, buffer, buffer_size);
                        return ELF_RESULT_ERR_FS;
                    }

                    size_t count = vm_copy_to(as, phdr->vaddr, buffer, phdr->filesz);
                    ASSERT(count == phdr->filesz);

                    vm_unmap(g_vm_global_address_space, buffer, buffer_size);
                }
                break;
            case ELF64_PT_NULL:   break;
            case ELF64_PT_INTERP: break;
            case ELF64_PT_PHDR:   break;
            default:              log(LOG_LEVEL_WARN, "ELF", "ignoring program header %#x", phdr->type); break;
        }
    }
    return ELF_RESULT_OK;
}
