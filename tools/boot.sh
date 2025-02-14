#!/usr/bin/env bash
clear

build_args=()
build_args+=(--hide-conflicts)
build_args+=(source/kernel target/kernel target/image)

while [[ $# -gt 0 ]]; do
    case $1 in
        --efi)
            BOOT_EFI="yes"
            ;;
        --production)
            build_args+=(--var "build_environment=production")
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

chariot "${build_args[@]}"
if [[ "$BOOT_EFI" = "yes" ]]; then
    IMAGE_PATH=./.chariot-cache/target/image/install/elysium_efi.img
else
    IMAGE_PATH=./.chariot-cache/target/image/install/elysium_bios.img
fi

qemu_args=()
qemu_args+=(-drive format=raw,file=$IMAGE_PATH)
qemu_args+=(-m 512M)
qemu_args+=(-machine q35)
qemu_args+=(-cpu qemu64,pdpe1gb)
qemu_args+=(-smp cores=4)
qemu_args+=(-vga virtio)
qemu_args+=(-display gtk,zoom-to-fit=on,show-tabs=on,gl=on)
qemu_args+=(-D ./log.txt)
qemu_args+=(-d int)
qemu_args+=(-M smm=off)
qemu_args+=(-k en-us)
qemu_args+=(-debugcon file:/dev/stdout)
qemu_args+=(-monitor stdio)
qemu_args+=(-no-reboot)
qemu_args+=(-no-shutdown)
qemu_args+=(-net none)
qemu_args+=(-accel kvm)
# qemu_args+=(-accel tcg)
# qemu_args+=(-s -S)

if [[ "$BOOT_EFI" = "yes" ]]; then
    qemu-ovmf-x86-64 "${qemu_args[@]}"
else
    qemu-system-x86_64 "${qemu_args[@]}"
fi
