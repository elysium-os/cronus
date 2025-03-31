#include "elf.h"

#include "arch/page.h"
#include "auxv.h"
#include "common/abi/sysv/elf64.h"
#include "common/assert.h"
#include "common/log.h"
#include "fs/vfs.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "lib/param.h"
#include "memory/heap.h"
#include "memory/vm.h"

#include <stddef.h>

#define ID0 0x7F
#define ID1 'E'
#define ID2 'L'
#define ID3 'F'

#define ELF_X86_64_MACHINE_386 0x3E

#define ELF_X86_64_PT_TLS 7

static void auto_free_header(elf64_file_header_t **header) {
    heap_free(*header, sizeof(header));
}

static void auto_free_phdr(elf64_phdr_t **phdr) {
    heap_free(*phdr, sizeof(phdr));
}

elf_result_t elf_load(vfs_node_t *file, vm_address_space_t *as, PARAM_OUT(char **) interpreter, PARAM_OUT(x86_64_auxv_t *) auxv) {
    if(file->type != VFS_NODE_TYPE_FILE) return ELF_RESULT_ERR_NOT_ELF;

    vfs_node_attr_t attr;
    vfs_result_t res = file->ops->attr(file, &attr);
    if(res != VFS_RESULT_OK) return ELF_RESULT_ERR_FS;
    if(attr.size < sizeof(elf64_file_header_t)) return ELF_RESULT_ERR_NOT_ELF;

    [[gnu::cleanup(auto_free_header)]] elf64_file_header_t *header = heap_alloc(sizeof(elf64_file_header_t));

    size_t read_count;
    res = file->ops->rw(file, &(vfs_rw_t) { .rw = VFS_RW_READ, .size = sizeof(elf64_file_header_t), .buffer = (void *) header }, &read_count);
    if(res != VFS_RESULT_OK || read_count != sizeof(elf64_file_header_t)) return ELF_RESULT_ERR_FS;

    if(header->ident.magic[0] != ID0 || header->ident.magic[1] != ID1 || header->ident.magic[2] != ID2 || header->ident.magic[3] != ID3) return ELF_RESULT_ERR_NOT_ELF;
    if(header->ident.class != ELF64_CLASS64) return ELF_RESULT_ERR_NOT_64BIT;
    if(header->ident.encoding != ELF64_DATA2LSB) return ELF_RESULT_ERR_NOT_LITTLE_ENDIAN;
    if(header->version > 1) return ELF_RESULT_ERR_UNSUPPORTED;
    if(header->machine != ELF_X86_64_MACHINE_386) return ELF_RESULT_ERR_NOT_X86_64;
    if(header->phentsize < sizeof(elf64_phdr_t)) return ELF_RESULT_ERR_UNSUPPORTED;

    size_t interpreter_size = 0;
    if(interpreter != NULL) *interpreter = NULL;

    [[gnu::cleanup(auto_free_phdr)]] elf64_phdr_t *phdr = heap_alloc(header->phentsize);
    for(int i = 0; i < header->phnum; i++) {
        res = file->ops->rw(file, &(vfs_rw_t) { .rw = VFS_RW_READ, .size = header->phentsize, .offset = header->phoff + (i * header->phentsize), .buffer = (void *) phdr }, &read_count);
        if(res != VFS_RESULT_OK || read_count != header->phentsize) {
            heap_free(interpreter, interpreter_size);
            return ELF_RESULT_ERR_FS;
        }

        switch(phdr->type) {
            case ELF64_PT_NULL: break;
            case ELF64_PT_LOAD:
                if(phdr->filesz > phdr->memsz) {
                    heap_free(interpreter, interpreter_size);
                    return ELF_RESULT_ERR_INVALID_PHDR;
                }

                vm_protection_t prot = { .read = true };
                if(phdr->flags & ELF64_PF_R) log(LOG_LEVEL_WARN, "ELF", "no-read program headers not supported");
                if(phdr->flags & ELF64_PF_W) prot.write = true;
                if(phdr->flags & ELF64_PF_X) prot.exec = true;

                uintptr_t aligned_vaddr = MATH_FLOOR(phdr->vaddr, ARCH_PAGE_GRANULARITY);
                size_t length = MATH_CEIL(phdr->memsz + (phdr->vaddr - aligned_vaddr), ARCH_PAGE_GRANULARITY);

                void *ptr = vm_map_anon(as, (void *) aligned_vaddr, length, prot, VM_CACHE_STANDARD, VM_FLAG_FIXED | VM_FLAG_ZERO);
                ASSERT(ptr != NULL);
                if(phdr->filesz > 0) {
                    size_t buffer_size = MATH_CEIL(phdr->filesz, ARCH_PAGE_GRANULARITY);
                    void *buffer = vm_map_anon(g_vm_global_address_space, NULL, buffer_size, VM_PROT_RW, VM_CACHE_STANDARD, VM_FLAG_NO_DEMAND);
                    ASSERT(buffer != NULL);

                    res = file->ops->rw(file, &(vfs_rw_t) { .rw = VFS_RW_READ, .size = phdr->filesz, .offset = phdr->offset, .buffer = buffer }, &read_count);
                    if(res != VFS_RESULT_OK || read_count != phdr->filesz) {
                        vm_unmap(g_vm_global_address_space, buffer, buffer_size);
                        heap_free(interpreter, interpreter_size);
                        return ELF_RESULT_ERR_FS;
                    }
                    size_t copyto_count = vm_copy_to(as, phdr->vaddr, buffer, phdr->filesz);
                    ASSERT(copyto_count == phdr->filesz);
                    vm_unmap(g_vm_global_address_space, buffer, buffer_size);
                }
                break;
            case ELF64_PT_INTERP:
                ASSERT(interpreter != NULL);

                interpreter_size = phdr->filesz + 1;
                char *interp = heap_alloc(interpreter_size);
                memset(interp, 0, phdr->filesz + 1);
                res = file->ops->rw(file, &(vfs_rw_t) { .rw = VFS_RW_READ, .buffer = interp, .offset = phdr->offset, .size = phdr->filesz }, &read_count);
                if(res != VFS_RESULT_OK) {
                    heap_free(interp, interpreter_size);
                    return ELF_RESULT_ERR_FS;
                }
                *interpreter = interp;
                break;
            case ELF64_PT_PHDR: auxv->phdr = phdr->vaddr; break;
            default:            log(LOG_LEVEL_WARN, "ELF", "ignoring program header %#x", phdr->type); break;
        }
    }

    auxv->entry = header->entry;
    auxv->phent = header->phentsize;
    auxv->phnum = header->phnum;

    return ELF_RESULT_OK;
}
