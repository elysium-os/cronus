#include "sys/module.h"

#include "abi/sysv/elf64.h"
#include "arch/elf.h"
#include "arch/page.h"
#include "common/assert.h"
#include "common/buffer.h"
#include "common/log.h"
#include "fs/vfs.h"
#include "lib/container.h"
#include "lib/list.h"
#include "lib/math.h"
#include "lib/param.h"
#include "lib/string.h"
#include "memory/heap.h"
#include "memory/vm.h"
#include "sys/kernel_symbol.h"

#include <stddef.h>
#include <stdint.h>

static void auto_free_header(elf64_file_header_t **header) {
    heap_free(*header, sizeof(header));
}

static void auto_free_shdr(elf64_section_header_t **shdr) {
    heap_free(*shdr, sizeof(shdr));
}

static void auto_free_module(module_t **auto_module) {
    module_t *module = *auto_module;
    if(module == nullptr) return;

    while(module->module_regions.count > 0) {
        module_region_t *region = CONTAINER_OF(list_pop(&module->module_regions), module_region_t, list_node);
        vm_unmap(g_vm_global_address_space, region->base, region->size);
        heap_free(region, sizeof(module_region_t));
    }

    heap_free(module, sizeof(module_t));
}

static bool read_section(vfs_node_t *file, elf64_section_header_t *shdr, PARAM_OUT(buffer_t **) buffer) {
    *buffer = buffer_alloc(shdr->size);
    size_t read_count;
    vfs_result_t res = file->ops->rw(file, &(vfs_rw_t) { .rw = VFS_RW_READ, .size = shdr->size, .offset = shdr->offset, .buffer = &((*buffer)->data) }, &read_count);
    return res != VFS_RESULT_OK || read_count != shdr->size;
}

module_result_t module_load(vfs_node_t *module_file, PARAM_OUT(module_t **) module) {
    if(!kernel_symbols_is_loaded()) return MODULE_RESULT_ERR_NO_KERNEL_SYMBOLS;

    [[gnu::cleanup(auto_free_header)]] elf64_file_header_t *header = heap_alloc(sizeof(elf64_file_header_t));

    size_t read_count;
    vfs_result_t res;

    res = module_file->ops->rw(module_file, &(vfs_rw_t) { .rw = VFS_RW_READ, .size = sizeof(elf64_file_header_t), .buffer = (void *) header }, &read_count);
    if(res != VFS_RESULT_OK) return MODULE_RESULT_ERR_FS;
    if(read_count != sizeof(elf64_file_header_t)) return MODULE_RESULT_ERR_NOT_MODULE;

    if(!ELF64_ID_VALIDATE(header->ident.magic)) return MODULE_RESULT_ERR_NOT_MODULE;
    if(header->ident.class != ARCH_ELF_CLASS) return MODULE_RESULT_ERR_INVALID_CLASS;
    if(header->ident.encoding != ARCH_ELF_ENCODING) return MODULE_RESULT_ERR_INVALID_ENCODING;
    if(header->machine != ARCH_ELF_MACHINE) return MODULE_RESULT_ERR_INVALID_MACHINE;
    if(header->type != ELF64_ET_REL) return MODULE_RESULT_ERR_INVALID_TYPE;
    if(header->version > 1) return MODULE_RESULT_ERR_UNSUPPORTED;
    if(header->shnum == 0 || header->shentsize < sizeof(elf64_section_header_t)) return MODULE_RESULT_ERR_UNSUPPORTED;

    // load sections
    [[gnu::cleanup(auto_free_shdr)]] elf64_section_header_t *shdrs = heap_alloc(sizeof(elf64_section_header_t) * header->shnum);
    for(size_t i = 1; i < header->shnum; i++) {
        res = module_file->ops->rw(module_file, &(vfs_rw_t) { .rw = VFS_RW_READ, .size = sizeof(elf64_section_header_t), .offset = header->shoff + (i * header->shentsize), .buffer = &shdrs[i] }, &read_count);
        if(res != VFS_RESULT_OK || read_count != sizeof(elf64_section_header_t)) return MODULE_RESULT_ERR_FS;
    }

    [[gnu::cleanup(buffer_cleanup)]] buffer_t *section_strtab = nullptr;
    if(header->shstrndx != ELF64_SHN_UNDEF) {
        if(read_section(module_file, &shdrs[header->shstrndx], &section_strtab)) return MODULE_RESULT_ERR_FS;
    }

    [[gnu::cleanup(buffer_cleanup)]] buffer_t *section_addresses = buffer_alloc(sizeof(uintptr_t) * header->shnum);
    buffer_clear(section_addresses);

    // load module into memory
    [[gnu::cleanup(auto_free_module)]] module_t *temp_module = heap_alloc(sizeof(module_t));
    temp_module->initialize = nullptr;
    temp_module->uninitialize = nullptr;
    temp_module->module_regions = LIST_INIT;

    size_t symtab_index = ELF64_SHN_UNDEF;
    for(size_t i = 1; i < header->shnum; i++) {
        switch(shdrs[i].type) {
            case ELF64_SHT_PROGBITS:
                if(shdrs[i].size == 0) {
                    log(LOG_LEVEL_WARN, "MODULE", "ignoring section header `%s` (progbits) because it has null size", section_strtab == nullptr ? "no shstrndx" : (char *) &section_strtab->data[shdrs[i].name]);
                    continue;
                }

                vm_protection_t prot = { .read = true, .write = true }; // TODO: remap memory as unwritable once data is read
                if((shdrs[i].flags & ELF64_SHF_WRITE) != 0) prot.write = true;
                if((shdrs[i].flags & ELF64_SHF_EXECINSTR) != 0) prot.exec = true;

                size_t aligned_size = MATH_CEIL(shdrs[i].size, PAGE_GRANULARITY);
                void *addr = vm_map_anon(g_vm_global_address_space, nullptr, aligned_size, prot, VM_CACHE_STANDARD, VM_FLAG_NONE);
                if(addr == nullptr) return MODULE_RESULT_ERR_VM;

                module_region_t *region = heap_alloc(sizeof(module_region_t));
                region->base = addr;
                region->size = aligned_size;
                list_push(&temp_module->module_regions, &region->list_node);

                res = module_file->ops->rw(module_file, &(vfs_rw_t) { .rw = VFS_RW_READ, .size = shdrs[i].size, .offset = shdrs[i].offset, .buffer = addr }, &read_count);
                if(res != VFS_RESULT_OK || read_count != shdrs[i].size) return MODULE_RESULT_ERR_FS;

                ((uintptr_t *) &section_addresses->data)[i] = (uintptr_t) addr;
                break;
            case ELF64_SHT_SYMTAB: symtab_index = i; break;
            case ELF64_SHT_NULL:   break;
            case ELF64_SHT_STRTAB: break;
            case ELF64_SHT_RELA:   break;
            case ELF64_SHT_REL:    return MODULE_RESULT_ERR_UNSUPPORTED;
            default:               log(LOG_LEVEL_WARN, "MODULE", "ignoring section header `%s` %u", section_strtab == nullptr ? "no shstrndx" : (char *) &section_strtab->data[shdrs[i].name], shdrs[i].type); break;
        }
    }
    if(symtab_index == ELF64_SHN_UNDEF) return MODULE_RESULT_ERR_MISSING_SYMTAB;

    // load symbol table
    [[gnu::cleanup(buffer_cleanup)]] buffer_t *symbols = nullptr;
    [[gnu::cleanup(buffer_cleanup)]] buffer_t *symbol_strtab = nullptr;

    if(read_section(module_file, &shdrs[shdrs[symtab_index].link], &symbol_strtab)) return MODULE_RESULT_ERR_FS;
    if(read_section(module_file, &shdrs[symtab_index], &symbols)) return MODULE_RESULT_ERR_FS;

    // resolve symbols
    for(size_t i = 1; i < shdrs[symtab_index].size / shdrs[symtab_index].entsize; i++) {
        elf64_symbol_t *symbol = (void *) &symbols->data[shdrs[symtab_index].entsize * i];

        switch(symbol->shndx) {
            case ELF64_SHN_UNDEF:
                kernel_symbol_t kernel_symbol;
                if(!kernel_symbol_lookup_by_name((char *) &symbol_strtab->data[symbol->name], &kernel_symbol)) {
                    log(LOG_LEVEL_WARN, "MODULE", "unknown undef symbol `%s`", &symbol_strtab->data[symbol->name]);
                    return MODULE_RESULT_ERR_UNRESOLVED_SYMBOL;
                }
                symbol->value = kernel_symbol.address;
                break;
            case ELF64_SHN_ABS:    break;
            case ELF64_SHN_COMMON: log(LOG_LEVEL_WARN, "MODULE", "unexpected symbol in SHN_COMMON section"); break;
            default:
                uintptr_t section_load_addr = ((uintptr_t *) &section_addresses->data)[symbol->shndx];
                ASSERT(section_load_addr != 0);
                symbol->value += section_load_addr;
                break;
        }
    }

    // relocations
    for(int i = 1; i < header->shnum; i++) {
        if(shdrs[i].type != ELF64_SHT_RELA) continue;

        // Ignore non-allocated sections
        if((shdrs[shdrs[i].info].flags & ELF64_SHF_ALLOC) == 0) continue;

        if(symtab_index != shdrs[i].link) {
            log(LOG_LEVEL_WARN, "MODULE", "relocations found for a non-symtab section");
            continue;
        }

        [[gnu::cleanup(buffer_cleanup)]] buffer_t *rela_buffer = nullptr;
        if(read_section(module_file, &shdrs[i], &rela_buffer)) return MODULE_RESULT_ERR_FS;

        for(size_t j = 0; j < shdrs[i].size / shdrs[i].entsize; j++) {
            elf64_rela_t *rela = (void *) &rela_buffer->data[shdrs[i].entsize * j];
            elf64_symbol_t *symbol = (void *) &symbols->data[shdrs[symtab_index].entsize * ELF64_R_SYM(rela->info)];

            uintptr_t section_address = ((uintptr_t *) &section_addresses->data)[shdrs[i].info];
            ASSERT(section_address != 0);

            if(!elf_do_relocation_addend(rela, symbol, section_address)) return MODULE_RESULT_ERR_INVALID_RELOCATION;
        }
    }

    for(size_t j = 1; j < shdrs[symtab_index].size / shdrs[symtab_index].entsize; j++) {
        elf64_symbol_t *symbol = (void *) &symbols->data[shdrs[symtab_index].entsize * j];

        if(string_eq((void *) &symbol_strtab->data[symbol->name], "__module_initialize")) temp_module->initialize = (void (*)()) symbol->value;
        if(string_eq((void *) &symbol_strtab->data[symbol->name], "__module_uninitialize")) temp_module->uninitialize = (void (*)()) symbol->value;
    }

    *module = temp_module;
    temp_module = nullptr;
    return MODULE_RESULT_OK;
}

const char *module_result_stringify(module_result_t result) {
    switch(result) {
        case MODULE_RESULT_OK:                     return "ok"; break;
        case MODULE_RESULT_ERR_FS:                 return "filesystem error"; break;
        case MODULE_RESULT_ERR_VM:                 return "virtual memory error"; break;
        case MODULE_RESULT_ERR_NO_KERNEL_SYMBOLS:  return "missing kernel modules"; break;
        case MODULE_RESULT_ERR_NOT_MODULE:         return "not a module"; break;
        case MODULE_RESULT_ERR_INVALID_TYPE:       return "invalid elf type"; break;
        case MODULE_RESULT_ERR_INVALID_CLASS:      return "invalid elf class"; break;
        case MODULE_RESULT_ERR_INVALID_ENCODING:   return "invalid elf encoding"; break;
        case MODULE_RESULT_ERR_INVALID_MACHINE:    return "invalid elf machine"; break;
        case MODULE_RESULT_ERR_INVALID_RELOCATION: return "invalid relocation"; break;
        case MODULE_RESULT_ERR_MISSING_SYMTAB:     return "missing symbol table"; break;
        case MODULE_RESULT_ERR_UNRESOLVED_SYMBOL:  return "unresolved symbol"; break;
        case MODULE_RESULT_ERR_UNSUPPORTED:        return "unsupported"; break;
    }
    ASSERT_UNREACHABLE();
}
