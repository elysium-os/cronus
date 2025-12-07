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

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(8);

    ld_cpu_local_init_start = .;

    .cpu_local_init : {
        *(.cpu_local_init)
    } :rodata

    ld_cpu_local_init_end = .;

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .data : {
        *(.data .data.*)
    } :data

    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    . = ALIGN(8);

    ld_cpu_local_bsp_start = .;
    . += ld_cpu_local_init_end - ld_cpu_local_init_start;
    ld_cpu_local_bsp_end = .;

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
