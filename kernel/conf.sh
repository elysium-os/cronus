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
            ;;
        --toolchain-triplet=*)
            TC_TRIPLET=${1#*=}
            ;;
        --production)
            ENVIRONMENT="production"
            ;;
        -*|--*)
            echo "Unknown option \"$1\""
            exit 1
            ;;
        *)
            POSITIONAL_ARGS+=("$1") # save positional arg
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

SRCDIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
DSTDIR=$(pwd)

cp $SRCDIR/Makefile $DSTDIR

CONFMK="$DSTDIR/conf.mk"
echo "SRC := $SRCDIR/src" > $CONFMK
echo "SUPPORT := $SRCDIR/support" >> $CONFMK
echo "BUILD := $DSTDIR/build" >> $CONFMK
echo "ASMC := $TC_TRIPLET-as" >> $CONFMK
echo "CC := $TC_TRIPLET-gcc" >> $CONFMK
echo "LD := $TC_TRIPLET-ld" >> $CONFMK
echo "PREFIX := $PREFIX" >> $CONFMK
echo "ARCH := $ARCH" >> $CONFMK
echo "ENVIRONMENT := $ENVIRONMENT" >> $CONFMK
echo "SYSROOT := $SYSROOT" >> $CONFMK