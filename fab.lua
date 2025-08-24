local version = "alpha.6"

-- Options
local opt_arch = fab.option("arch", { "x86_64" }) or "x86_64"
local opt_build_type = fab.option("buildtype", { "debug", "release" }) or "debug"

local opt_build_kernel = fab.option("build_kernel", { "yes", "no" }) or "yes"
local opt_build_modules = fab.option("build_modules", "string") or ""

-- Sources
local kernel_sources = sources(fab.glob("kernel/**/*.c", "kernel/arch/**"))

table.extend(kernel_sources, sources(fab.glob(path("kernel/arch", opt_arch, "**/*.c"))))

if opt_arch == "x86_64" then
    table.extend(kernel_sources, sources(fab.glob("kernel/arch/x86_64/**/*.asm")))
end

-- Includes
local include_dirs = {
    builtins.c.include_dir("kernel/include"),
    builtins.c.include_dir(path("kernel/arch", opt_arch, "include")),
    builtins.c.include_dir("kernel/arch/include")
}

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
    "-Wimplicit-fallthrough",
    "-Wmissing-field-initializers",

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
    "https://github.com/osdev0/freestnd-c-hdrs.git",
    "4039f438fb1dc1064d8e98f70e1cf122f91b763b"
)

local cc_runtime = fab.dependency(
    "cc-runtime",
    "https://github.com/osdev0/cc-runtime.git",
    "dae79833b57a01b9fd3e359ee31def69f5ae899b"
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

table.extend(kernel_sources, sources(path(cc_runtime.path, "src/cc-runtime.c")))

local uacpi_sources = sources(uacpi:glob("source/*.c"))

-- Tools
local cc = builtins.c.get_compiler("clang")
if cc == nil then
    error("No viable C compiler found")
end

local linker = builtins.get_linker()
if linker == nil then
    error("No viable linker found")
end

-- Modules
local modules = {}
if opt_build_modules ~= "" then
    for _, module in ipairs(fab.string_split(opt_build_modules, " ")) do
        modules[module] = fab.source(path("modules", module .. ".c"))
    end
end

if opt_arch == "x86_64" then
    -- Tools
    local nasm = builtins.nasm.get_assembler(nil, false)
    if nasm == nil then
        error("No NASM assembler found")
    end

    --- Includes
    table.insert(include_dirs, builtins.c.include_dir(path(freestanding_c_headers.path, "x86_64/include")))

    -- Flags
    table.extend(c_flags, {
        "--target=x86_64-none-elf",
        "-mno-red-zone",
        "-mgeneral-regs-only",
        "-mabi=sysv"
    })

    if opt_build_type == "debug" then
        table.insert(c_flags, "-fno-sanitize=alignment")
    end

    local nasm_flags = { "-f", "elf64", "-Werror" }

    local cflags_kernel = { "-mcmodel=kernel", table.unpack(c_flags) }
    local cflags_uacpi = table.shallow_clone(cflags_kernel)
    local cflags_module = { "-mcmodel=large", "-fno-pic", table.unpack(c_flags) }
    if opt_build_type == "debug" then
        table.insert(cflags_kernel, "-Werror")
        table.insert(cflags_module, "-Werror")
    end

    -- Build Kernel
    if opt_build_kernel == "yes" then
        local objects = builtins.generate(kernel_sources, {
            c = function(sources) return cc:generate(sources, cflags_kernel, include_dirs) end,
            asm = function(sources) return nasm:generate(sources, nasm_flags) end
        })
        table.extend(objects, cc:generate(uacpi_sources, cflags_uacpi, include_dirs))

        local kernel = linker:link("kernel.elf", objects, {
            "-T" .. fab.path_rel("kernel/support/link.x86_64.ld"),
            "-znoexecstack"
        })

        kernel:install("kernel.elf")
    end

    -- Build Modules
    for name, source in pairs(modules) do
        cc:compile_object(name, source, include_dirs, cflags_module):install("modules/" .. name .. ".cronmod")
    end
end
