add_languages('nasm', native : false)

sources += files(
    'abi/sysv/sysv.c',
    'abi/sysv/elf.c',
    'cpu/cpu.c',
    'cpu/cpuid.c',
    'cpu/fpu.c',
    'cpu/gdt.c',
    'cpu/lapic.c',
    'cpu/tss.c',
    'dev/hpet.c',
    'dev/ioapic.c',
    'dev/pic8259.c',
    'dev/pit.c',
    'dev/qemu_debug.c',
    'debug.c',
    'debug_prof.c',
    'exception.c',
    'init.c',
    'interrupt.c',
    'ptm.c',
    'sched.c',
    'syscall.c'
)

sources += files(
    'cpu/gdt.asm',
    'interrupt.asm',
    'mem.asm',
    'sched.asm',
    'syscall.asm'
)

kernel_cflags += [
    '--target=x86_64-none-elf',
    '-mcmodel=kernel',
    '-mno-red-zone',
    '-mgeneral-regs-only',
    '-mabi=sysv'
]

if get_option('buildtype') == 'debug'
    kernel_cflags += [
        '-fno-sanitize=alignment'
    ]
endif

kernel_ldflags += [
    '-Wl,-T' + meson.project_source_root() / 'kernel/support/link.x86_64.ld',
    '-Wl,-znoexecstack'
]

executable(
    'kernel.elf',
    dependencies : deps,
    sources : sources,
    objects : objects,
    include_directories : includes,
    c_args : kernel_cflags,
    nasm_args : [ '-f elf64', '-Werror' ],
    link_args : kernel_ldflags,
    install : true
)
