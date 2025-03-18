#!/usr/bin/env bash

case $1 in
    vscode)
        mkdir -p .vscode
        cat > .vscode/c_cpp_properties.json <<EOF
{
    "configurations": [
        {
            "name": "ElysiumOS",
            "includePath": [
                "\${workspaceFolder}/kernel/src",
                "\${workspaceFolder}/.chariot-cache/target/tartarus/install/usr/include",
                "\${workspaceFolder}/.chariot-cache/source/uacpi/src/include"
            ],
            "defines": [
                "__ARCH_X86_64",
                "__ENV_DEVELOPMENT",

                "UACPI_NATIVE_ALLOC_ZEROED",
                "UACPI_FORMATTED_LOGGING",
                "UACPI_SIZED_FREES",

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
                "-std=gnu2x",
                "-ffreestanding",
                "-mcmodel=kernel",
                "-mno-red-zone",
                "-mgeneral-regs-only",
                "-mabi=sysv",

                "-Wall",
                "-Wextra",
                "-Wvla",
                "-Wshadow",

                "-fno-stack-protector",
                "-fno-stack-check",
                "-fno-omit-frame-pointer",
                "-fno-strict-aliasing",
                "-fno-lto"
            ]
        }
    ],
    "version": 4
}
EOF

    cat > .vscode/settings.json <<EOF
{
    "C_Cpp.files.exclude": {
        "**/.chariot-cache": true
    },
    "C_Cpp.autoAddFileAssociations": false,
    "[c]": {
        "editor.formatOnSave": true
    },
    "todo-tree.general.tags": [
        "TODO",
        "todo",
        "OPTIMIZE",
        "UNIMPLEMENTED",
        "CRITICAL",
        "FIX",
        "TEMPORARY",
        "NOTE",
    ],
    "todo-tree.highlights.backgroundColourScheme": [
        "yellow",
        "yellow",
        "purple",
        "orange",
        "red",
        "coral",
        "cyan",
        "grey"
    ],
    "todo-tree.highlights.foregroundColourScheme": [
        "black",
        "black",
        "white",
        "black",
        "white",
        "black",
        "black",
        "black"
    ],
    "todo-tree.highlights.customHighlight": {
        "TODO": { "icon": "check" }
    },
    "todo-tree.regex.regex": "(//|#|@|<!--|;|/\\*|^|^[ \\t]*(-|\\d+.))\\s*($TAGS)"
}
EOF
        ;;
    zed)
        ;;
    *)
        echo "Unknown IDE \"$1\", available is vscode/zed"
        exit 1
        ;;
esac

