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
        --toolchain-triplet=*)
            TC_TRIPLET=${1#*=}
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

if [ -z "$TC_TRIPLET" ]; then
    >&2 echo "No toolchain triplet provided"
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

echo "ASMC := $TC_TRIPLET-as" >> $CONFIG_FILE
echo "CC := $TC_TRIPLET-gcc" >> $CONFIG_FILE
echo "LD := $TC_TRIPLET-ld" >> $CONFIG_FILE
echo "AR := $TC_TRIPLET-ar" >> $CONFIG_FILE