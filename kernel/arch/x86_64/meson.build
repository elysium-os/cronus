add_languages('nasm')

sources += files(
    'abi/sysv/sysv.c',
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

cflags += [
    '--target=x86_64-none-elf',
    '-mcmodel=kernel',
    '-mno-red-zone',
    '-mgeneral-regs-only',
    '-mabi=sysv',
    '-D__ARCH_X86_64',
    '-D__ARCH=x86_64'
]

if get_option('buildtype') == 'debug'
    cflags += [ '-fno-sanitize=alignment' ]
endif

ldflags += ['-Wl,-T' + meson.source_root() / 'support/link.x86_64.ld']

executable(
    'kernel.elf',
    sources : sources,
    objects : objects,
    include_directories : includes,
    c_args : cflags,
    nasm_args : [ '-f elf64', '-Werror' ],
    link_args : ldflags,
    install : true
)
