#!/bin/bash

# Compile the custom kernel module
cd custom_module
make clean
make
make install
cd ..

# Compile the test program
rm -f qemu_shared/test
rm -f qemu_shared/test_multithread

gcc -static -o qemu_shared/test qemu_shared/test.c
gcc -static -o qemu_shared/test_multithread qemu_shared/test_multithread.c

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL=linux-6.19.6/arch/x86_64/boot/bzImage
INITRAMFS=busybox/initramfs.cpio.gz
INSTALL_DIR="$SCRIPT_DIR/busybox/_install"

# Build initramfs
echo "[*] Building initramfs..."
(cd "$INSTALL_DIR" && find . | cpio -H newc -o | gzip -9 > "$INITRAMFS")

qemu-system-x86_64 \
  -kernel "$KERNEL" \
  -initrd "$INITRAMFS" \
  -append "console=ttyS0 root=/dev/ram0 rw init=/sbin/init" \
  -m 512M \
  -nographic \
  -enable-kvm \
  -fsdev local,security_model=passthrough,id=fsdev0,path=qemu_shared \
  -device virtio-9p-pci,fsdev=fsdev0,mount_tag=shared