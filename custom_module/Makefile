KERNEL_DIR=../linux-6.19.6
INITRAMFS_DIR=../busybox/initramfs_dir
INITRAMFS_IMG=../busybox/initramfs.cpio.gz
KERNEL_VERSION=$(shell cat $(KERNEL_DIR)/include/config/kernel.release 2>/dev/null || make -s -C $(KERNEL_DIR) kernelrelease)


obj-m = my_module.o
module = my_module.ko

all:
	make -C $(KERNEL_DIR) M=`pwd` modules

install:
	mkdir -p $(INITRAMFS_DIR)/lib/modules/$(KERNEL_VERSION)
	cp $(module) $(INITRAMFS_DIR)/lib/modules/$(KERNEL_VERSION)/
	cd $(INITRAMFS_DIR) && find . | cpio -o -H newc | gzip > ../initramfs.cpio.gz

clean:
	make -C $(KERNEL_DIR) M=`pwd` clean
	rm -f $(INITRAMFS_DIR)/lib/modules/$(KERNEL_VERSION)/*