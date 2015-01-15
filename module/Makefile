ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m := shfs.o

shfs-objs := dcache.o dir.o fcache.o file.o inode.o ioctl.o proc.o shell.o symlink.o

else
# external module build

ifndef KERNEL
KERNEL=$(shell uname -r)
endif

ifndef MODULESDIR
MODULESDIR=${ROOT}/lib/modules/${KERNEL}
endif

ifndef KERNEL_SOURCES
KERNEL_SOURCES=${MODULESDIR}/build
endif

LINVER := linux-${KERNEL}
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNEL_SOURCES) SUBDIRS=$(PWD) modules

clean:
	rm -f *.o *.ko *.mod.c .*o.cmd modules.order Module.symvers

install: shfs.ko
	rm -f ${MODULESDIR}/kernel/fs/shfs/shfs.ko
	install -m644 -b -D shfs.ko ${MODULESDIR}/kernel/fs/shfs/shfs.ko
	if [ -x /sbin/depmod -a "${ROOT}" = "/" ]; then /sbin/depmod -aq; fi

uninstall:
	rm -rf ${MODULESDIR}/kernel/fs/shfs
	if [ -x /sbin/depmod -a "${ROOT}" = "/" ]; then /sbin/depmod -aq; fi

endif

.PHONY : all clean install uninstall 
