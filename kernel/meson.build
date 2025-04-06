sources += files(
    'abi/sysv/elf.c',
    'common/lock/mutex.c',
    'common/lock/spinlock.c',
    'common/buffer.c',
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
    'sys/kernel_symbol.c',
    'sys/time.c',
    'sys/module.c',
    'syscall/syscall.c',
    'syscall/syscall_mem.c',
    'ring_buffer.c',
    'terminal.c',
    'ubsan.c'
)

includes += include_directories('.')

subdir('arch' / get_option('arch'))
