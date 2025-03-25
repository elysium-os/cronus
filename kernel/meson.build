project('Elysium', 'c')

sources = []
objects = []
includes = []

cflags = [
    '-std=gnu23',
    '-ffreestanding',

    '-Wall',
    '-Wextra',
    '-Wvla',
    '-Wshadow',

    '-fno-stack-protector',
    '-fno-stack-check',
    '-fno-strict-aliasing',

    '-DUACPI_NATIVE_ALLOC_ZEROED',
    '-DUACPI_FORMATTED_LOGGING',
    '-DUACPI_SIZED_FREES'
]

ldflags = [
    '-nostdlib',
    '-static'
]

includes += include_directories('.')
if get_option('external_headers') != ''
    includes += include_directories(get_option('external_headers'))
endif

if get_option('buildtype') == 'release'
    cflags += [ '-O3', '-flto', '-D__ENV_PRODUCTION' ]
endif

if get_option('buildtype') == 'debug'
    cflags += [
        '-O0',
        '-g',
        '-fsanitize=undefined',
        '-finstrument-functions',
        '-fno-lto',
        '-fno-omit-frame-pointer',
        '-D__ENV_DEVELOPMENT'
    ]
endif

if get_option('uacpi_path') != ''
    objects += files(get_option('uacpi_path'))
endif

sources += files(
    'common/lock/mutex.c',
    'common/lock/spinlock.c',
    'common/log.c',
    'common/panic.c',
    'dev/acpi/acpi.c',
    'dev/acpi/uacpi.c',
    'dev/pci.c',
    'fs/rdsk.c',
    'fs/tmpfs.c',
    'fs/vfs.c',
    'graphics/draw.c',
    'graphics/font_basic.c',
    'lib/format.c',
    'lib/list.c',
    'lib/mem.c',
    'lib/string.c',
    'memory/heap.c',
    'memory/pmm.c',
    'memory/slab.c',
    'memory/vm.c',
    'sched/process.c',
    'sched/reaper.c',
    'sched/sched.c',
    'sys/time.c',
    'syscall/syscall.c',
    'syscall/syscall_mem.c',
    'ring_buffer.c',
    'terminal.c',
    'ubsan.c'
)

subdir('arch/' / get_option('arch'))
