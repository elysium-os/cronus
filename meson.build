project('Cronus', 'c', version : 'alpha.6')

deps = []
sources = []
objects = []
includes = []

kernel_cflags = [
    '-std=gnu23',
    '-ffreestanding',
    '-nostdinc',

    '-Wall',
    '-Wextra',
    '-Wvla',
    '-Wshadow',

    '-fno-stack-protector',
    '-fno-stack-check',
    '-fno-strict-aliasing',

    '-DUACPI_NATIVE_ALLOC_ZEROED',
    '-DUACPI_FORMATTED_LOGGING',
    '-DUACPI_SIZED_FREES',

    '-D__VERSION=' + meson.project_version(),
    '-D__ARCH_' + get_option('arch').to_upper(),
    '-D__ARCH=' + get_option('arch')
]

kernel_ldflags = [
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

deps += dependency(
	'freestnd-c-hdrs-' + host_machine.cpu_family(),
	required: true,
	fallback: ['freestnd-c-hdrs', 'freestnd_c_hdrs_dep']
)

deps += dependency(
	'cc-runtime',
	required: true,
	fallback: ['cc-runtime', 'cc_runtime_deps']
)

deps += dependency(
	'tartarus-bootloader',
	required: true,
	fallback: ['tartarus-bootloader', 'tartarus_bootloader_deps']
)

uacpi = subproject('uacpi')
sources += uacpi.get_variable('sources')
includes += uacpi.get_variable('includes')

subdir('kernel')
