OUTPUT_ARCH(i386:x86-64)
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(init)

PHDRS
{
    text    PT_LOAD FLAGS((1 << 0) | (1 << 2));
    data    PT_LOAD FLAGS((1 << 1) | (1 << 2));
    rodata  PT_LOAD FLAGS((1 << 2));
}

SECTIONS
{
    . = 0xFFFFFFFF80000000;

    ld_kernel_start = .;

    .text : {
        *(.text .text.*)
    } :text

    . += CONSTANT(MAXPAGESIZE);

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    ld_cpu_local_init_start = .;

    .cpu_local_init : {
        *(.cpu_local_init)
    } :rodata

    ld_cpu_local_init_end = .;

    . += CONSTANT(MAXPAGESIZE);

    .data : {
        *(.data .data.*)
    } :data

    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    ld_init_targets_start = .;

    .init_targets : {
        *(.init_targets .init_targets.*)
    } :data

    ld_init_targets_end = .;

    ld_kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note .note.*)
    }
}
