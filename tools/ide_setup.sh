#!/usr/bin/env bash

# **Cursed** script for generating IDE config

# kernel config
KERN_DEFINES[0]=__ARCH_X86_64
KERN_DEFINES[1]=__ENV_DEVELOPMENT
KERN_DEFINES[3]=__VERSION=meson_defined
KERN_DEFINES[4]=__ARCH=meson_defined
KERN_DEFINES[5]=UACPI_NATIVE_ALLOC_ZEROED
KERN_DEFINES[6]=UACPI_FORMATTED_LOGGING
KERN_DEFINES[7]=UACPI_SIZED_FREES

KERN_INCLUDES[0]=kernel
KERN_INCLUDES[1]=.chariot-cache/source/tartarus/src
KERN_INCLUDES[2]=.chariot-cache/source/uacpi/src/include
KERN_INCLUDES[3]=.chariot-cache/source/mlibc-sysdeps/src/elysium/include
KERN_INCLUDES[4]=.chariot-cache/source/freestanding_headers/src

KERN_FLAGS[0]=-std=gnu2x
KERN_FLAGS[1]=-ffreestanding
KERN_FLAGS[2]=-nostdinc
KERN_FLAGS[3]=-mcmodel=kernel
KERN_FLAGS[4]=-mno-red-zone
KERN_FLAGS[5]=-mgeneral-regs-only
KERN_FLAGS[6]=-mabi=sysv

KERN_FLAGS[7]=-Wall
KERN_FLAGS[8]=-Wextra
KERN_FLAGS[9]=-Wvla
KERN_FLAGS[10]=-Wshadow

KERN_FLAGS[11]=-fno-stack-protector
KERN_FLAGS[12]=-fno-stack-check
KERN_FLAGS[13]=-fno-omit-frame-pointer
KERN_FLAGS[14]=-fno-strict-aliasing
KERN_FLAGS[15]=-fno-lto

# mlibc sysdep config
SYSDEP_INCLUDES[0]=.chariot-cache/target/mlibc/build/ld.a.p
SYSDEP_INCLUDES[1]=.chariot-cache/target/mlibc/build
SYSDEP_INCLUDES[2]=.chariot-cache/source/mlibc/src
SYSDEP_INCLUDES[3]=.chariot-cache/source/mlibc/src/options/internal/include
SYSDEP_INCLUDES[4]=.chariot-cache/source/mlibc/src/options/internal/x86_64-include
SYSDEP_INCLUDES[5]=.chariot-cache/source/mlibc/src/options/rtld/x86_64
SYSDEP_INCLUDES[6]=.chariot-cache/source/mlibc/src/options/rtld/include
SYSDEP_INCLUDES[7]=.chariot-cache/source/mlibc/src/sysdeps/elysium/include
SYSDEP_INCLUDES[8]=.chariot-cache/source/mlibc/src/sysdeps/generic-helpers/include
SYSDEP_INCLUDES[9]=.chariot-cache/source/mlibc/src/options/ansi/include
SYSDEP_INCLUDES[10]=.chariot-cache/source/mlibc/src/options/elf/include
SYSDEP_INCLUDES[11]=.chariot-cache/source/freestanding_c_headers/src/x86_64/include
SYSDEP_INCLUDES[12]=.chariot-cache/source/freestanding_cxx_headers/src/x86_64/include
SYSDEP_INCLUDES[13]=.chariot-cache/source/frigg/src/include

SYSDEP_FLAGS[0]=-std=c++20

# init program config
INIT_INCLUDES[0]=.chariot-cache/target/mlibc_headers/install/usr/include

INIT_FLAGS[0]=-std=gnu2x
INIT_FLAGS[1]=-static
INIT_FLAGS[2]=-O0
INIT_FLAGS[3]=-g

# zed setup
setup_zed() {
    cat > .clangd <<EOF
If:
    PathMatch: kernel/.*
CompileFlags:
    Add:
    - "-xc"
$(for FLAG in ${KERN_FLAGS[@]}; do echo -e "    - \"$FLAG\""; done)
$(for DEF in ${KERN_DEFINES[@]}; do echo -e "    - \"-D$DEF\""; done)
$(for INC in ${KERN_INCLUDES[@]}; do echo -e "    - \"-I$(readlink -f $INC)\""; done)
---
If:
    PathMatch: mlibc-sysdeps/.*
CompileFlags:
    Add:
$(for FLAG in ${SYSDEP_FLAGS[@]}; do echo -e "    - \"$FLAG\""; done)
$(for INC in ${SYSDEP_INCLUDES[@]}; do echo -e "    - \"-I$(readlink -f $INC)\""; done)
---
If:
    PathMatch: init/.*
CompileFlags:
    Add:
$(for FLAG in ${INIT_FLAGS[@]}; do echo -e "    - \"$FLAG\""; done)
$(for INC in ${INIT_INCLUDES[@]}; do echo -e "    - \"-I$(readlink -f $INC)\""; done)
EOF

mkdir -p .zed
cat > .zed/settings.json <<EOF
{
    "lsp": {
        "clangd": {
            "binary": {
                "path": "$(which clangd)"
            }
        }
    }
}
EOF
}

# vscode setup
setup_vscode() {
    mkdir -p .vscode
    cat > .vscode/c_cpp_properties.json <<EOF
{
    "configurations": [
        {
            "name": "ElysiumOS",
            "includePath": [
$(for INC in ${KERN_INCLUDES[@]}; do echo -e "               \"\${workspaceFolder}/$INC\","; done)
            ],
            "defines": [
$(for DEF in ${KERN_DEFINES[@]}; do echo -e "               \"$DEF\","; done)

                /* Unsupported C23 features work-around */
                /* https://github.com/microsoft/vscode-cpptools/issues/10696 */
                "true=1",
                "false=0",
                "bool=_Bool",
                "static_assert=_Static_assert"
            ],
            "compilerPath": "$(which clang)",
            "cStandard": "gnu23",
            "intelliSenseMode": "clang-x64",
            "compilerArgs": [
$(for FLAG in ${KERN_FLAGS[@]}; do echo -e "               \"$FLAG\","; done)
            ]
        }
    ],
    "version": 4
}
EOF

    cat > .vscode/settings.json <<EOF
{
    "[c]": { "editor.formatOnSave": true },
    "C_Cpp.files.exclude": { "**/.chariot-cache": true },
    "C_Cpp.autoAddFileAssociations": false,
    "todo-tree.general.tags": [
        "TODO", "todo", "OPTIMIZE", "UNIMPLEMENTED",
        "CRITICAL", "FIX", "TEMPORARY", "NOTE",
    ],
    "todo-tree.highlights.backgroundColourScheme": [
        "yellow", "yellow", "purple", "orange",
        "red", "coral", "cyan", "grey"
    ],
    "todo-tree.highlights.foregroundColourScheme": [
        "black", "black", "white", "black",
        "white", "black", "black", "black"
    ],
    "todo-tree.highlights.customHighlight": { "TODO": { "icon": "check" } },
    "todo-tree.regex.regex": "(//|#|@|<!--|;|/\\\\*|^|^[ \\\\t]*(-|\\\\d+.))\\\\s*(\$TAGS)"
}
EOF
}

# build
chariot target/tartarus source/uacpi target/mlibc_headers source/freestanding_headers

# generate config
case $1 in
    zed)
        setup_zed
        ;;
    vscode)
        setup_vscode
        ;;
    *)
        echo "Unknown IDE \"$1\", available is vscode/zed"
        exit 1
        ;;
esac
