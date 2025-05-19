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

-- Generator Helper
local function gen_objects(sources, mappings)
    local mapped = {}
    for _, source in ipairs(sources) do
        for extension, generator in pairs(mappings) do
            if source.name:ends_with("." .. extension) then
                mapped[extension] = mapped[extension] or { generator = generator, sources = {} }
                table.insert(mapped[extension].sources, source)
            end
        end
    end

    local objects = {}
    for _, m in pairs(mapped) do
        table.extend(objects, m.generator(m.sources))
    end

    return objects
end

-- Modules
local modules = {}
for _, module in ipairs(fab.string_split(opt_build_modules, " ")) do
    modules[module] = fab.source(path("modules", module .. ".c"))
end

if opt_arch == "x86_64" then
    local asmc = builtins.nasm.get_assembler()
    if asmc == nil then
        error("No NASM assembler found")
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

    local nasm_flags = { "-f", "elf64", "-Werror" }

    -- Build Kernel
    if opt_build_kernel == "yes" then
        local objects = gen_objects(kernel_sources, {
            c = function(sources) return cc:compile_objects(sources, include_dirs, c_flags) end,
            asm = function(sources) return asmc:assemble(sources, nasm_flags) end
        })

        local kernel = linker:link("kernel.elf", objects, {
            "-T" .. fab.path_rel(path(fab.project_root(), "kernel/support/link.x86_64.ld")),
            "-znoexecstack"
        })

        kernel:install("share/kernel.elf")
    end

    -- Build Modules
    for name, source in pairs(modules) do
        local objects = cc:compile_objects({ source }, include_dirs, c_flags)
        objects[1]:install("share/modules/" .. name .. ".kmod")
    end
end
