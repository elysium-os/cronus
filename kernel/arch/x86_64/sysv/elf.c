#include "elf.h"

#include "auxv.h"
#include "common/assert.h"
#include "common/log.h"
#include "fs/vfs.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "lib/param.h"
#include "memory/heap.h"
#include "memory/vm.h"

#include <arch/page.h>
#include <stddef.h>

#define ID0 0x7F
#define ID1 'E'
#define ID2 'L'
#define ID3 'F'

#define LITTLE_ENDIAN 1
#define CLASS64 2
#define MACHINE_386 0x3E

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
#define PT_TLS 7

#define PF_X 0x1 /* Execute */
#define PF_W 0x2 /* Write */
#define PF_R 0x4 /* Read */

static_assert(sizeof(unsigned char) == 1);

typedef uint64_t elf64_addr_t;
typedef uint64_t elf64_off_t;
typedef uint16_t elf64_half_t;
typedef uint32_t elf64_word_t;
typedef int32_t elf64_sword_t;
typedef uint64_t elf64_xword_t;
typedef int64_t elf64_sxword_t;

typedef struct [[gnu::packed]] {
    unsigned char magic[4];
    unsigned char class;
    unsigned char encoding;
    unsigned char file_version;
    unsigned char abi;
    unsigned char abi_version;
    unsigned char rsv0[6];
    unsigned char nident;
} elf_identifier_t;

typedef struct [[gnu::packed]] {
    elf_identifier_t ident;
    elf64_half_t type;
    elf64_half_t machine;
    elf64_word_t version;
    elf64_addr_t entry;
    elf64_off_t phoff; /* program header offset */
    elf64_off_t shoff; /* section header offset */
    elf64_word_t flags;
    elf64_half_t ehsize; /* ELF Header size */
    elf64_half_t phentsize; /* program header entry size */
    elf64_half_t phnum; /* program header count */
    elf64_half_t shentsize; /* section header entry size */
    elf64_half_t shnum; /* section header count */
    elf64_half_t shstrndx; /* section name string table index */
} elf_header_t;

typedef struct [[gnu::packed]] {
    elf64_word_t type;
    elf64_word_t flags;
    elf64_off_t offset;
    elf64_addr_t vaddr; /* virtual address */
    elf64_addr_t paddr; /* physical address */
    elf64_xword_t filesz; /* file size */
    elf64_xword_t memsz; /* memory size */
    elf64_xword_t align; /* alignment */
} elf_phdr_t;

static void auto_free_header(elf_header_t **header) {
    heap_free(*header, sizeof(header));
}

static void auto_free_phdr(elf_phdr_t **phdr) {
    heap_free(*phdr, sizeof(phdr));
}

elf_result_t elf_load(vfs_node_t *file, vm_address_space_t *as, PARAM_OUT(char **) interpreter, PARAM_OUT(x86_64_auxv_t *) auxv) {
    if(file->type != VFS_NODE_TYPE_FILE) return ELF_RESULT_ERR_NOT_ELF;

    vfs_node_attr_t attr;
    vfs_result_t res = file->ops->attr(file, &attr);
    if(res != VFS_RESULT_OK) return ELF_RESULT_ERR_FS;
    if(attr.size < sizeof(elf_header_t)) return ELF_RESULT_ERR_NOT_ELF;

    [[gnu::cleanup(auto_free_header)]] elf_header_t *header = heap_alloc(sizeof(elf_header_t));

    size_t read_count;
    res = file->ops->rw(file, &(vfs_rw_t) { .rw = VFS_RW_READ, .size = sizeof(elf_header_t), .buffer = (void *) header }, &read_count);
    if(res != VFS_RESULT_OK || read_count != sizeof(elf_header_t)) return ELF_RESULT_ERR_FS;

    if(header->ident.magic[0] != ID0 || header->ident.magic[1] != ID1 || header->ident.magic[2] != ID2 || header->ident.magic[3] != ID3) return ELF_RESULT_ERR_NOT_ELF;
    if(header->ident.class != CLASS64) return ELF_RESULT_ERR_NOT_64BIT;
    if(header->ident.encoding != LITTLE_ENDIAN) return ELF_RESULT_ERR_NOT_LITTLE_ENDIAN;
    if(header->version > 1) return ELF_RESULT_ERR_UNSUPPORTED;
    if(header->machine != MACHINE_386) return ELF_RESULT_ERR_NOT_X86_64;
    if(header->phentsize < sizeof(elf_phdr_t)) return ELF_RESULT_ERR_UNSUPPORTED;

    size_t interpreter_size = 0;
    if(interpreter != NULL) *interpreter = NULL;

    [[gnu::cleanup(auto_free_phdr)]] elf_phdr_t *phdr = heap_alloc(header->phentsize);
    for(int i = 0; i < header->phnum; i++) {
        res = file->ops->rw(file, &(vfs_rw_t) { .rw = VFS_RW_READ, .size = header->phentsize, .offset = header->phoff + (i * header->phentsize), .buffer = (void *) phdr }, &read_count);
        if(res != VFS_RESULT_OK || read_count != header->phentsize) {
            heap_free(interpreter, interpreter_size);
            return ELF_RESULT_ERR_FS;
        }

        switch(phdr->type) {
            case PT_NULL: break;
            case PT_LOAD:
                if(phdr->filesz > phdr->memsz) {
                    heap_free(interpreter, interpreter_size);
                    return ELF_RESULT_ERR_INVALID_PHDR;
                }

                vm_protection_t prot = { .read = true };
                if(phdr->flags & PF_W) prot.write = true;
                if(phdr->flags & PF_X) prot.exec = true;

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
            case PT_INTERP:
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
            case PT_PHDR: auxv->phdr = phdr->vaddr; break;
            default:      log(LOG_LEVEL_WARN, "ELF", "Ignoring program header %#x", phdr->type); break;
        }
    }

    auxv->entry = header->entry;
    auxv->phent = header->phentsize;
    auxv->phnum = header->phnum;

    return ELF_RESULT_OK;
}
