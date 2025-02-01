#!/usr/bin/env bash
mkdir -p _usb

EFI_FILE=.chariot-cache/target/tartarus_efi/install/usr/share/tartarus/tartarus.efi
KERNEL_FILE=.chariot-cache/target/kernel/install/usr/share/kernel.elf
KERNEL_SYMBOLS_FILE=.chariot-cache/target/kernel/install/usr/share/kernel_symbols.txt

if [ ! -f $EFI_FILE ]; then
    chariot target/tartarus_efi
fi

if [ ! -f $KERNEL_FILE ]; then
    chariot target/kernel
fi

sudo mount $1 _usb

sudo rm -f _usb/kernel.elf
sudo cp $KERNEL_FILE _usb/kernel.elf
sudo rm -f _usb/kernel_symbols.txt
sudo cp $KERNEL_SYMBOLS_FILE _usb/kernel_symbols.txt

sudo rm -rf _usb/EFI
sudo mkdir -p _usb/EFI/BOOT
sudo cp $EFI_FILE _usb/EFI/BOOT/BOOTX64.EFI

sudo rm -f _usb/tartarus.cfg
sudo bash -c "cat > _usb/tartarus.cfg <<EOF
protocol = \"tartarus\"
kernel = \"kernel.elf\"
module = \"kernel_symbols.txt\"

smp = false

fb_width = 3840
fb_height = 2160
EOF"

sudo umount _usb

rm -fd _usb