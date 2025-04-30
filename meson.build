project('Cronus', 'c', version : 'alpha.6', meson_version : '>=1.2.0')

kernel_sources = []
kernel_includes = []
kernel_cflags = []
kernel_ldflags = []

kernel_includes += include_directories('kernel')

kernel_cflags += [
    '-std=gnu23',
    '-ffreestanding',
    '-nostdinc',

    '-fno-stack-protector',
    '-fno-stack-check',
    '-fno-strict-aliasing',

    '-Wall',
    '-Wextra',
    '-Wvla',
    '-Wshadow',

    '-D__VERSION=' + meson.project_version(),
    '-D__ARCH_' + get_option('arch').to_upper(),
    '-D__ARCH=' + get_option('arch')
]

kernel_cflags += [
    '-DUACPI_NATIVE_ALLOC_ZEROED',
    '-DUACPI_FORMATTED_LOGGING',
    '-DUACPI_SIZED_FREES'
]

kernel_ldflags += [
    '-nostdlib',
    '-static'
]

if get_option('buildtype') == 'release'
    kernel_cflags += [
        '-O3',
        '-flto',
        '-D__ENV_PRODUCTION',
        '-D__ENV=production'
    ]
endif

if get_option('buildtype') == 'debug'
    kernel_cflags += [
        '-O0',
        '-g',
        '-fsanitize=undefined',
        '-finstrument-functions',
        '-fno-lto',
        '-fno-omit-frame-pointer',
        '-D__ENV_DEVELOPMENT',
        '-D__ENV=development'
    ]
endif

dependencies = [
    dependency(
    	'freestnd-c-hdrs-' + host_machine.cpu_family(),
    	required: true,
    	fallback: ['freestnd-c-hdrs', 'freestnd_c_hdrs_dep']
    ),
    dependency(
    	'cc-runtime',
    	required: true,
    	fallback: ['cc-runtime', 'cc_runtime_deps']
    ),
    dependency(
        'tartarus-bootloader',
        required: true,
        fallback: ['tartarus-bootloader', 'tartarus_dep']
    )
]

uacpi = subproject('uacpi')
kernel_includes += uacpi.get_variable('includes')
if get_option('build_kernel')
    kernel_sources += uacpi.get_variable('sources')
endif

subdir('kernel')

foreach module : get_option('build_module')
    subdir('modules' / module)
endforeach
