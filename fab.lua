-- Options
local opt_arch = fab.option("arch", "x86_64")
local opt_build_type = fab.option("buildtype", "debug")

-- Sources
local kernel_sources = {}
for _, path in ipairs(fab.glob("kernel/**/*.c")) do
    if not path:starts_with("kernel/arch/") then
        table.insert(kernel_sources, fab.source(path))
    end
end

table.extend(kernel_sources, sources(fab.glob(path("kernel/arch", opt_arch, "**/*.c"))))

if opt_arch == "x86_64" then
    table.extend(kernel_sources, sources(fab.glob("kernel/arch/x86_64/**/*.asm")))
end

-- Includes
local include_dirs = includes("kernel")

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

    -- "-D__VERSION="" + meson.project_version(),
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

table.extend(include_dirs, includes(
    path(uacpi.path, "include"),
    tartarus_bootloader.path
))

table.extend(kernel_sources, sources(path(cc_runtime.path, "cc-runtime.c")))
table.extend(kernel_sources, sources(uacpi:glob("source/*.c")))

if opt_arch == "x86_64" then
    -- Find nasm
    local nasm = fab.find_executable("nasm")
    if nasm == nil then
        panic("nasm not found")
        return
    end

    local ASMC = fab.create_compiler {
        name = "asmc",
        executable = nasm,
        command = "@EXEC@ @FLAGS@ @IN@ -o @OUT@",
        description = "Assembling @OUT@"
    }

    -- Find LD
    local ld = fab.find_executable("ld.lld")
    if ld == nil then
        panic("ld.lld not found")
        return
    end

    local LD = fab.create_linker {
        name = "ld",
        executable = ld,
        command = "@EXEC@ @FLAGS@ -o @OUT@ @IN@",
        description = "Linking @OUT@"
    }

    -- Separate sources
    local asm_sources = {}
    local c_sources = {}

    for _, source in ipairs(kernel_sources) do
        if source.filename:ends_with(".asm") then
            table.insert(asm_sources, source)
        end
        if source.filename:ends_with(".c") then
            table.insert(c_sources, source)
        end
    end

    --- Includes
    table.extend(include_dirs, includes(path(freestanding_c_headers.path, "x86_64/include")))

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
    table.extend(objects, CC:build(c_sources, c_flags, include_dirs))
    table.extend(objects, ASMC:build(asm_sources, { "-f", "elf64", "-Werror" }))

    -- Link
    LD:link(objects, { "-T" .. path(fab.project_root(), "kernel/support/link.x86_64.ld"), "-znoexecstack" }, "kernel.elf")
end
