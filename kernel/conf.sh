#!/bin/sh

PREFIX="/usr/local"
ENVIRONMENT="development"

while [[ $# -gt 0 ]]; do
    case $1 in
        --prefix=*)
            PREFIX=${1#*=}
            ;;
        --sysroot=*)
            SYSROOT=${1#*=}
            ;;
        --arch=*)
            ARCH=${1#*=}
            case $ARCH in
                x86_64)
                    ;;
                *)
                    echo "Unknown architecture \"$ARCH\""
                    exit 1
                    ;;
            esac
            ;;
        --production)
            ENVIRONMENT="production"
            ;;
        --toolchain-nasm=*)
            TC_NASM=${1#*=}
            ;;
        --toolchain-compiler=*)
            TC_COMPILER=${1#*=}
            ;;
        --toolchain-linker=*)
            TC_LINKER=${1#*=}
            ;;
        -*|--*)
            echo "Unknown option \"$1\""
            exit 1
            ;;
        *)
            POSITIONAL_ARGS+=("$1")
            ;;
    esac
    shift
done
set -- "${POSITIONAL_ARGS[@]}"

if [ -z "$ARCH" ]; then
    >&2 echo "No architecture provided"
    exit 1
fi

if [ "$ARCH" = "x86_64" ] && [ -z "$TC_NASM" ]; then
    >&2 echo "No nasm provided"
    exit 1
fi

if [ -z "$TC_COMPILER" ]; then
    >&2 echo "No compiler provided"
    exit 1
fi

if [ -z "$TC_LINKER" ]; then
    >&2 echo "No linker provided"
    exit 1
fi

SRC_DIRECTORY=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
DEST_DIRECTORY=$(pwd)

cp $SRC_DIRECTORY/Makefile $DEST_DIRECTORY

CONFIG_FILE="$DEST_DIRECTORY/conf.mk"
echo "SRC_DIRECTORY := $SRC_DIRECTORY/src" > $CONFIG_FILE
echo "SUPPORT_DIRECTORY := $SRC_DIRECTORY/support" >> $CONFIG_FILE
echo "BUILD_DIRECTORY := $DEST_DIRECTORY/build" >> $CONFIG_FILE
echo "PREFIX := $PREFIX" >> $CONFIG_FILE
echo "SYSROOT := $SYSROOT" >> $CONFIG_FILE
echo "ARCH := $ARCH" >> $CONFIG_FILE
echo "ENVIRONMENT := $ENVIRONMENT" >> $CONFIG_FILE

echo "NASM := $TC_NASM" >> $CONFIG_FILE
echo "CC := $TC_COMPILER" >> $CONFIG_FILE
echo "LD := $TC_LINKER" >> $CONFIG_FILE
