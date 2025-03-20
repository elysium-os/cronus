#!/usr/bin/env bash

# **Cursed** script for generating IDE config

DEFINES[0]=__ARCH_X86_64
DEFINES[1]=__ENV_DEVELOPMENT
DEFINES[2]=UACPI_NATIVE_ALLOC_ZEROED
DEFINES[3]=UACPI_FORMATTED_LOGGING
DEFINES[4]=UACPI_SIZED_FREES

INCLUDES[0]=kernel/src
INCLUDES[1]=.chariot-cache/target/tartarus/install/usr/include
INCLUDES[2]=.chariot-cache/source/uacpi/src/include

FLAGS[0]=-std=gnu2x
FLAGS[1]=-ffreestanding
FLAGS[2]=-mcmodel=kernel
FLAGS[3]=-mno-red-zone
FLAGS[4]=-mgeneral-regs-only
FLAGS[5]=-mabi=sysv

FLAGS[6]=-Wall
FLAGS[7]=-Wextra
FLAGS[8]=-Wvla
FLAGS[9]=-Wshadow

FLAGS[10]=-fno-stack-protector
FLAGS[11]=-fno-stack-check
FLAGS[12]=-fno-omit-frame-pointer
FLAGS[13]=-fno-strict-aliasing
FLAGS[14]=-fno-lto

setup_zed() {
    cat > .clangd <<EOF
CompileFlags:
    Add:
    - "-xc"
$(for FLAG in ${FLAGS[@]}; do echo -e "    - \"$FLAG\""; done)
$(for DEF in ${DEFINES[@]}; do echo -e "    - \"-D$DEF\""; done)
$(for INC in ${INCLUDES[@]}; do echo -e "    - \"-I$(readlink -f $INC)\""; done)
EOF
}

setup_vscode() {
    mkdir -p .vscode
    cat > .vscode/c_cpp_properties.json <<EOF
{
    "configurations": [
        {
            "name": "ElysiumOS",
            "includePath": [
$(for INC in ${INCLUDES[@]}; do echo -e "               \"\${workspaceFolder}/$INC\","; done)
            ],
            "defines": [
$(for DEF in ${DEFINES[@]}; do echo -e "               \"$DEF\","; done)

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
$(for FLAG in ${FLAGS[@]}; do echo -e "               \"$FLAG\","; done)
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

chariot target/tartarus source/uacpi

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
