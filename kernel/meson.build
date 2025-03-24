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

includes += include_directories('src')
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

subdir('src')
