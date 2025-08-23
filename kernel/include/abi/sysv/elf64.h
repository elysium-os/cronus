#pragma once

#include <stdint.h>

#define ELF64_ID0 0x7F
#define ELF64_ID1 'E'
#define ELF64_ID2 'L'
#define ELF64_ID3 'F'
#define ELF64_ID_VALIDATE(ID) ((ID)[0] == ELF64_ID0 && (ID)[1] == ELF64_ID1 && (ID)[2] == ELF64_ID2 && (ID)[3] == ELF64_ID3)

#define ELF64_CLASS32 1
#define ELF64_CLASS64 2

#define ELF64_DATA2LSB 1 /* little endian */
#define ELF64_DATA2MSB 2 /* big endian */

#define ELF64_OSABI_SYSV 0
#define ELF64_OSABI_HPUX 1
#define ELF64_OSABI_STANDALONE 255

#define ELF64_ET_NONE 0 /* no type */
#define ELF64_ET_REL 1 /* relocatable */
#define ELF64_ET_EXEC 2 /* executable */
#define ELF64_ET_DYN 3 /* shared object */
#define ELF64_ET_CORE 4 /* core */
#define ELF64_ET_LOOS 0xFE00 /* environment specific use */
#define ELF64_ET_HIOS 0xFEFF
#define ELF64_ET_LOPROC 0xFF00 /* processor specific use */
#define ELF64_ET_HIPROC 0xFFFF

#define ELF64_SHN_UNDEF 0 /* undefined/meaningless section reference */
#define ELF64_SHN_LOPROC 0xFF00 /* processor specific use */
#define ELF64_SHN_HIPROC 0xFF1F
#define ELF64_SHN_LOOS 0xFF20 /* environment specific use */
#define ELF64_SHN_HIOS 0xFF3F
#define ELF64_SHN_ABS 0xFFF1 /* reference is absolute value */
#define ELF64_SHN_COMMON 0xFFF2

#define ELF64_SHT_NULL 0 /* unused */
#define ELF64_SHT_PROGBITS 1 /* contains information defined by program */
#define ELF64_SHT_SYMTAB 2 /* contains linker symbol table */
#define ELF64_SHT_STRTAB 3 /* contains string table */
#define ELF64_SHT_RELA 4 /* contains "rela" relocation entries */
#define ELF64_SHT_HASH 5 /* contains symbol hash table */
#define ELF64_SHT_DYNAMIC 6 /* contains dynamic linking tables */
#define ELF64_SHT_NOTE 7 /* contains note info */
#define ELF64_SHT_NOBITS 8 /* contains uninitialized space */
#define ELF64_SHT_REL 9 /* contains "rel" relocation entries */
#define ELF64_SHT_SHLIB 10 /* reserved */
#define ELF64_SHT_DYNSYM 11 /* contains dynamic loader symbol table */
#define ELF64_SHT_LOOS 0x60000000 /* environment specific use */
#define ELF64_SHT_HIOS 0x6FFFFFFF
#define ELF64_SHT_LOPROC 0x70000000 /* processor specific use */
#define ELF64_SHT_HIPROC 0x7FFFFFFF

#define ELF64_SHF_WRITE (1 << 0) /* contains writable memory */
#define ELF64_SHF_ALLOC (1 << 1) /* allocated in program image */
#define ELF64_SHF_EXECINSTR (1 << 2) /* contains executable instructions */
#define ELF64_SHF_MASKOS 0x0F000000 /* environment specific use */
#define ELF64_SHF_MASKPROC 0xF0000000 /* processor specific use */

#define ELF64_STB_LOCAL 0
#define ELF64_STB_GLOBAL 1
#define ELF64_STB_WEAK 2
#define ELF64_STB_LOOS 3 /* environment specific use */
#define ELF64_STB_HIOS 4
#define ELF64_STB_LOPROC 5 /* processor specific use */
#define ELF64_STB_HIPROC 6

#define ELF64_STT_NOTYPE 0 /* no type */
#define ELF64_STT_OBJECT 1 /* data object */
#define ELF64_STT_FUNC 2 /* function entry point */
#define ELF64_STT_SECTION 3 /* associated section */
#define ELF64_STT_FILE 4 /* associated source file */
#define ELF64_STT_LOOS 10 /* environment specific use */
#define ELF64_STT_HIOS 12
#define ELF64_STT_LOPROC 13 /* processor specific use */
#define ELF64_STT_HIPROC 15

#define ELF64_PT_NULL 0 /* unused */
#define ELF64_PT_LOAD 1 /* loadable segment */
#define ELF64_PT_DYNAMIC 2 /* dynamic linking table */
#define ELF64_PT_INTERP 3 /* program interpreter path */
#define ELF64_PT_NOTE 4 /* note sections */
#define ELF64_PT_SHLIB 5 /* reserved */
#define ELF64_PT_PHDR 6 /* program header table */
#define ELF64_PT_LOOS 0x60000000 /* environment specific use */
#define ELF64_PT_HIOS 0x6FFFFFFF
#define ELF64_PT_LOPROC 0x70000000 /* processor specific use */
#define ELF64_PT_HIPROC 0x7FFFFFFF

#define ELF64_PF_X (1 << 0) /* exec permission */
#define ELF64_PF_W (1 << 1) /* write permission */
#define ELF64_PF_R (1 << 2) /* read permission */
#define ELF64_PF_MASKOS 0x00FF0000 /* environment specific use */
#define ELF64_PF_MASKPROC 0xFF000000 /* processor specific use */

#define ELF64_DT_NULL 0 /* end of dynamic array */
#define ELF64_DT_NEEDED 1 /* string table offset of needed library */
#define ELF64_DT_PLTRELSZ 2 /* size of relocation entries associated with procedure linkage table */
#define ELF64_DT_PLTGOT 3 /* address of associated linkage table */
#define ELF64_DT_HASH 4 /* symbol hash table address */
#define ELF64_DT_STRTAB 5 /* dynamic string table address */
#define ELF64_DT_SYMTAB 6 /* dynamic symbol table address */
#define ELF64_DT_RELA 7 /* rela relocation table address */
#define ELF64_DT_RELASZ 8 /* size of rela relocation table */
#define ELF64_DT_RELAENT 9 /* size of rela entries */
#define ELF64_DT_STRSZ 10 /* size of string table */
#define ELF64_DT_SYMENT 11 /* size of symbol table entries */
#define ELF64_DT_INIT 12 /* address of initialization function */
#define ELF64_DT_FINI 13 /* address of termination function */
#define ELF64_DT_SONAME 14 /* string table offset of this shared object name */
#define ELF64_DT_RPATH 15 /* string table offset of a shared library search path */
#define ELF64_DT_SYMBOLIC 16 /* see spec */
#define ELF64_DT_REL 17 /* rel relocation table address */
#define ELF64_DT_RELSZ 18 /* size of rel relocation table */
#define ELF64_DT_RELENT 19 /* size of rel entries */
#define ELF64_DT_PLTREL 20 /* relocation type use for procedure linkage table */
#define ELF64_DT_DEBUG 21 /* reserved for debugger */
#define ELF64_DT_TEXTREL 22 /* see spec */
#define ELF64_DT_JMPREL 23 /* address of relocations associated with procedure linkage table */
#define ELF64_DT_BIND_NOW 24 /* see spec */
#define ELF64_DT_INIT_ARRAY 25 /* pointer to array of initialization function pointers */
#define ELF64_DT_FINI_ARRAY 26 /* pointer to array of termination function pointers */
#define ELF64_DT_INIT_ARRAYSZ 27 /* init array size */
#define ELF64_DT_FINI_ARRAYSZ 28 /* fini array size */
#define ELF64_DT_LOOS 0x60000000 /* environment specific use */
#define ELF64_DT_HIOS 0x6FFFFFFF
#define ELF64_DT_LOPROC 0x70000000 /* processor specific use */
#define ELF64_DT_HIPROC 0x7FFFFFFF

#define ELF64_R_SYM(I) ((I) >> 32)
#define ELF64_R_TYPE(I) ((I) & 0xFFFFFFFFL)
#define ELF64_R_INFO(S, T) (((S) << 32) + ((T) & 0xFFFFFFFFL))

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
} elf64_identifier_t;

typedef struct [[gnu::packed]] {
    elf64_identifier_t ident;
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
} elf64_file_header_t;

typedef struct [[gnu::packed]] {
    elf64_word_t name;
    elf64_word_t type;
    elf64_xword_t flags;
    elf64_addr_t addr;
    elf64_off_t offset;
    elf64_xword_t size;
    elf64_word_t link;
    elf64_word_t info;
    elf64_xword_t addralign;
    elf64_xword_t entsize;
} elf64_section_header_t;

typedef struct [[gnu::packed]] {
    elf64_word_t name;
    unsigned char info; /* type and binding attributes */
    unsigned char other; /* reserved */
    elf64_half_t shndx; /* section table index */
    elf64_addr_t value;
    elf64_xword_t size;
} elf64_symbol_t;

typedef struct [[gnu::packed]] {
    elf64_addr_t offset;
    elf64_xword_t addr;
} elf64_rel_t;

typedef struct [[gnu::packed]] {
    elf64_addr_t offset;
    elf64_xword_t info;
    elf64_sxword_t addend;
} elf64_rela_t;

typedef struct [[gnu::packed]] {
    elf64_word_t type;
    elf64_word_t flags;
    elf64_off_t offset;
    elf64_addr_t vaddr; /* virtual address */
    elf64_addr_t paddr; /* physical address */
    elf64_xword_t filesz; /* file size */
    elf64_xword_t memsz; /* memory size */
    elf64_xword_t align; /* alignment */
} elf64_program_header_t;

typedef struct [[gnu::packed]] {
    elf64_sxword_t tag;
    union {
        elf64_xword_t val;
        elf64_addr_t ptr;
    } un;
} elf64_dynamic_t;

static inline unsigned long elf64_hash(const unsigned char *name) {
    unsigned long h = 0, g;
    while(*name != '\0') {
        h = (h << 4) + *name++;
        if((g = (h & 0xF0000000))) h ^= (g >> 24);
        h &= 0x0FFFFFFF;
    }
    return h;
}
