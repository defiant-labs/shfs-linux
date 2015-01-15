# which kernel version to use
KERNEL=$(shell uname -r)
# where to install module
MODULESDIR=${ROOT}/lib/modules/${KERNEL}
# kernel source tree
KERNEL_SOURCES=${MODULESDIR}/build
# leave empty if you don't need man pages installed
MAN_PAGE_DIR=/usr/share/man
# leave empty if you don't need html manual
HTML_DOC_DIR=

export ROOT KERNEL MODULESDIR KERNEL_SOURCES MODVERSIONS MAN_PAGE_DIR TMP

SHFS_VERSION=$(shell head -n1 Changelog |sed "s/^[^(]*(\([^)]*\)).*$$/\1/")

export SHFS_VERSION

default: all

all: module utils docs
clean:  module-clean utils-clean docs-clean
install: module-install utils-install docs-install
uninstall: module-uninstall utils-uninstall docs-uninstall

utils:
	@make -C tool
	
utils-clean:
	@make -C tool clean

utils-install:
	@make -C tool install
	
utils-uninstall:
	@make -C tool uninstall

module:
	@make -C module

module-clean:
	@make -C module clean

module-install:
	@make -C module install

module-uninstall:
	@make -C module uninstall

docs:
	@make -C docs

docs-clean:
	@make -C docs clean

docs-install:
	@make -C docs install

docs-uninstall:
	@make -C docs uninstall

dist: distclean clean rpm-spec deb-changelog
	@make -C docs
	mkdir shfs-${SHFS_VERSION}
	rm -f shfs-${SHFS_VERSION}.tar.gz
	find . -maxdepth 1 ! -name . ! -name "shfs-${SHFS_VERSION}" -exec cp -R {} shfs-${SHFS_VERSION}/ \;
	tar -czf shfs-${SHFS_VERSION}.tar.gz --exclude=CVS --exclude=amd --exclude=html/*.in --exclude=linkbar --exclude=logo shfs-${SHFS_VERSION}
	rm -rf shfs-${SHFS_VERSION}

distclean:
	rm -f shfs-${SHFS_VERSION}.tar.gz

deb-control:
	sed s%@@KERNEL@@%${KERNEL}%g debian/control.in >debian/control
	
deb-control-clean:
	rm -f debian/control
	
deb: deb-changelog deb-control
	dpkg-buildpackage -rfakeroot -us -uc -tc

deb-clean: deb-changelog-clean deb-control-clean
	if [ -f ./build-stamp ]; then \
		fakeroot debian/rules clean; \
	fi

deb-changelog:
	ln -fs ../Changelog debian/changelog

deb-changelog-clean:
	rm -f debian/changelog

rpm: dist
	make -C rpm rpm

rpm-clean:
	if [ -f rpm/shfs-module.spec -o -f rpm/shfs-utils.spec ]; then \
		make -C rpm rpm-clean; \
	fi

rpm-spec:
	make -C rpm spec

rpm-spec-clean:
	make -C rpm spec-clean

.PHONY: default all clean install uninstall \
	utils utils-clean utils-install utils-uninstall \
        module module-clean module-install module-uninstall \
	docs docs-clean docs-install docs-uninstall \
	dist distclean deb deb-clean \
	deb-control deb-control-clean deb-changelog deb-changelog-clean \
	rpm rpm-clean rpm-spec rpm-spec-clean
