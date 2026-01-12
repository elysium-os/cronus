local ld = require("ld")
local c = require("lang_c")
local nasm = require("lang_nasm")

local version = "alpha.6"

-- Options
local options = {
    arch = fab.option("arch", { "x86_64" }) or "x86_64" --[[@as string]],
    build_type = fab.option("buildtype", { "debug", "release" }) or "debug" --[[@as string]],

    build_kernel = (function(opt) return (opt == nil) and true or opt end)(fab.option("build_kernel", "boolean")) --[[@as boolean]],
    build_modules = fab.option("build_modules", "string") or "" --[[@as string]],
}

print(options.build_kernel)

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

    "-fdiagnostics-color=always"
}

local c_flags_build_type = {
    debug = {
        "-O0",
        "-g",
        "-fsanitize=undefined",
        "-finstrument-functions",
        "-fno-lto",
        "-fno-omit-frame-pointer",
    },
    release = {
        "-O3",
        "-flto",
    },
}
table.extend(c_flags, c_flags_build_type[options.build_type])

local c_env_flags = {
    "-D__VERSION=" .. version,
    "-D__ARCH=" .. options.arch,
    "-D__ENV=" .. options.build_type,
    "-D__ARCH_" .. options.arch:upper(),
    "-D__ENV_" .. options.build_type:upper()
}

-- Sources
local kernel_sources = sources(
    fab.glob("**/*.c", "!arch/**", "!modules/**", "!**/support/**"),
    fab.glob(path("arch", options.arch, "**/*.c"), "!**/support/**")
)

if options.arch == "x86_64" then
    table.extend(kernel_sources, sources(fab.glob("arch/x86_64/**/*.asm", "!**/support/**")))
end

-- Includes
local include_dirs = {
    c.include_dir(path("arch", options.arch, "include")),
    c.include_dir("include")
}

-- Dependencies
local freestanding_c_headers = fab.git(
    "freestanding-c-headers",
    "https://github.com/osdev0/freestnd-c-hdrs.git",
    "4039f438fb1dc1064d8e98f70e1cf122f91b763b"
)

local cc_runtime = fab.git(
    "cc-runtime",
    "https://github.com/osdev0/cc-runtime.git",
    "dae79833b57a01b9fd3e359ee31def69f5ae899b"
)

local tartarus_bootloader = fab.git(
    "tartarus",
    "https://github.com/elysium-os/tartarus-bootloader.git",
    "main"
)

local uacpi = fab.git(
    "uacpi",
    "https://github.com/uACPI/uACPI.git",
    "2.1.1"
)

table.extend(include_dirs, {
    c.include_dir(path(uacpi.path, "include")),
    c.include_dir(tartarus_bootloader.path),
})

table.extend(kernel_sources, sources(path(fab.build_dir(), cc_runtime.path, "src/cc-runtime.c")))

local uacpi_sources = sources(fab.glob("source/*.c", { relative_to = uacpi.path }))

table.extend(c_flags, {
    "-DUACPI_NATIVE_ALLOC_ZEROED",
    "-DUACPI_FORMATTED_LOGGING",
    "-DUACPI_SIZED_FREES"
})

-- Tools
local cc = c.get_clang()
if cc == nil then
    error("No viable C compiler found")
end

local linker = ld.get_linker("ld.lld")
if linker == nil then
    error("No viable linker found")
end

-- Modules
local modules = {}
if options.build_modules ~= "" then
    for _, module in ipairs(string.split(options.build_modules, " ")) do
        modules[module] = fab.def_source(path("modules", module .. ".c"))
    end
end

if options.arch == "x86_64" then
    -- Tools
    local nasm = nasm.get_nasm()
    if nasm == nil then
        error("No NASM assembler found")
    end

    --- Includes
    table.insert(include_dirs, c.include_dir(path(freestanding_c_headers.path, "x86_64/include")))

    -- ASM GEN
    local asmgen = cc:compile("x86_64_asmgen", sources("arch/x86_64/support/asmgen.c"),
        { "-std=gnu23", table.unpack(c_env_flags) },
        include_dirs)

    local asmgen_rule = fab.def_rule("asmgen", asmgen.path .. " @OUT@", "Generating X86_64 assembly includes")
    local asmgen_includes = asmgen_rule:build("x86_64_asm_defs.rsp", {}, {}, { asmgen })

    -- LD GEN
    local ldgen_rule = fab.def_rule(
        "ld-gen",
        fab.path_rel("arch/x86_64/support/ld-gen.py") .. " --template @TEMPLATE@ --dest @OUT@ @ARGS@ @IN@",
        "Generating ld script fragment for prefixed sections"
    )

    -- Flags
    table.extend(c_flags, {
        "--target=x86_64-none-elf",
        "-mno-red-zone",
        "-mgeneral-regs-only",
        "-mabi=sysv"
    })

    if options.build_type == "debug" then
        table.insert(c_flags, "-fno-sanitize=alignment")
    end

    local nasm_flags = { "-f", "elf64", "-Werror", "-@", asmgen_includes.path }

    local cflags_kernel = { "-mcmodel=kernel" }
    table.extend(cflags_kernel, c_flags)
    table.extend(cflags_kernel, c_env_flags)

    local cflags_module = { "-mcmodel=large", "-fno-pic" }
    table.extend(cflags_module, c_flags)
    table.extend(cflags_module, c_env_flags)

    local cflags_uacpi = { table.unpack(cflags_kernel) }

    if options.build_type == "debug" then
        table.insert(cflags_kernel, "-Werror")
        table.insert(cflags_module, "-Werror")
    end

    local install = {}

    -- Build Kernel
    if options.build_kernel then
        local objects = generate(kernel_sources, {
            c = function(sources) return cc:generate(sources, cflags_kernel, include_dirs) end,
            asm = function(sources) return nasm:generate(sources, nasm_flags, { asmgen_includes }) end
        })
        table.extend(objects, cc:generate(uacpi_sources, cflags_uacpi, include_dirs))

        local ld_script = ldgen_rule:build("x86_64-link.ld", objects, {
            template = fab.path_rel("arch/x86_64/support/link.ld.template"),
            args = table.join({
                "--opt", "arch=i386:x86-64",
                "--opt", "format=elf64-x86-64",
                "--opt", "kernel_start=0xFFFFFFFF80000000"
            }, " ")
        })

        local kernel = linker:link("kernel.elf", objects, {
            "-T" .. ld_script.path,
            "-znoexecstack"
        }, { ld_script })

        install["kernel.elf"] = kernel
    end

    -- Build Modules
    for name, source in pairs(modules) do
        install["modules/" .. name .. ".cronmod"] = cc:compile_object(name, source, include_dirs, cflags_module)
    end

    return { install = install }
end
