local version = "alpha.6"

-- Options
local opt_arch = fab.option("arch", { "x86_64" }) or "x86_64"
local opt_build_type = fab.option("buildtype", { "debug", "release" }) or "debug"

-- Sources
local kernel_sources = sources(fab.glob("kernel/**/*.c", "kernel/arch/**"))

table.extend(kernel_sources, sources(fab.glob(path("kernel/arch", opt_arch, "**/*.c"))))

if opt_arch == "x86_64" then
    table.extend(kernel_sources, sources(fab.glob("kernel/arch/x86_64/**/*.asm")))
end

-- Includes
local include_dirs = { builtins.c.include_dir("kernel") }

-- Flags
local c_flags = {
    "-std=gnu23",
    "-ffreestanding",
    "-nostdinc",

    "-fno-stack-protector",
    "-fno-stack-check",
    "-fno-strict-aliasing",

    "-Wall",
    "-Wextra",
    "-Wvla",
    "-Wshadow",

    "-D__VERSION=" .. version,
    "-D__ARCH_" .. opt_arch:upper(),
    "-D__ARCH=" .. opt_arch,

    "-fdiagnostics-color=always"
}

table.extend(c_flags, {
    "-DUACPI_NATIVE_ALLOC_ZEROED",
    "-DUACPI_FORMATTED_LOGGING",
    "-DUACPI_SIZED_FREES"
})

if opt_build_type == "release" then
    table.extend(c_flags, {
        "-O3",
        "-flto",
        "-D__ENV_PRODUCTION",
        "-D__ENV=production"
    })
end

if opt_build_type == "debug" then
    table.extend(c_flags, {
        "-O0",
        "-g",
        "-fsanitize=undefined",
        "-finstrument-functions",
        "-fno-lto",
        "-fno-omit-frame-pointer",
        "-D__ENV_DEVELOPMENT",
        "-D__ENV=development"
    })
end

-- Dependencies
local freestanding_c_headers = fab.dependency(
    "freestanding-c-headers",
    "https://codeberg.org/osdev/freestnd-c-hdrs.git",
    "21b59ecd6ef67bb32f893da8288ce08a324d1986"
)

local cc_runtime = fab.dependency(
    "cc-runtime",
    "https://codeberg.org/osdev/cc-runtime.git",
    "d5425655388977fa12ff9b903e554a20b20c426e"
)

local tartarus_bootloader = fab.dependency(
    "tartarus",
    "https://github.com/elysium-os/tartarus-bootloader.git",
    "main"
)

local uacpi = fab.dependency(
    "uacpi",
    "https://github.com/uACPI/uACPI.git",
    "2.1.1"
)

table.extend(include_dirs, {
    builtins.c.include_dir(path(uacpi.path, "include")),
    builtins.c.include_dir(tartarus_bootloader.path),
})

table.extend(kernel_sources, sources(path(cc_runtime.path, "cc-runtime.c")))
table.extend(kernel_sources, sources(uacpi:glob("source/*.c")))

local cc = builtins.c.get_compiler()
if cc == nil then
    error("No viable C compiler found")
end

local linker = builtins.get_linker()
if linker == nil then
    error("No viable linker found")
end

if opt_arch == "x86_64" then
    local asmc = builtins.nasm.get_assembler()
    if asmc == nil then
        error("No NASM assembler found")
    end

    -- Separate sources
    local asm_sources = {}
    local c_sources = {}

    for _, source in ipairs(kernel_sources) do
        if source.name:ends_with(".asm") then
            table.insert(asm_sources, source)
        end
        if source.name:ends_with(".c") then
            table.insert(c_sources, source)
        end
    end

    --- Includes
    table.insert(include_dirs, builtins.c.include_dir(path(freestanding_c_headers.path, "x86_64/include")))

    -- Flags
    table.extend(c_flags, {
        "--target=x86_64-none-elf",
        "-mcmodel=kernel",
        "-mno-red-zone",
        "-mgeneral-regs-only",
        "-mabi=sysv"
    })

    if opt_build_type == "debug" then
        table.insert(c_flags, "-fno-sanitize=alignment")
    end

    -- Build
    local objects = {}
    table.extend(objects, cc:compile_objects(c_sources, include_dirs, c_flags))
    table.extend(objects, asmc:assemble(asm_sources, { "-f", "elf64", "-Werror" }))
    local kernel = linker:link("kernel.elf", objects, {
        "-T" .. path(fab.project_root(), "kernel/support/link.x86_64.ld"), "-znoexecstack"
    })

    kernel:install("share/kernel.elf")
end
