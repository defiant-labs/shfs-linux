ifndef SHFS_VERSION
SHFS_VERSION=unknown
endif

SHFSMOUNT := /usr/bin/shfsmount
SHFSUMOUNT := /usr/bin/shfsumount

ALL_TARGETS := shfsmount shfsumount

SEARCHDIRS := -I- -I. -I../module/

CC     := gcc
CFLAGS  = -g -Wall ${SEARCHDIRS} -DSHFS_VERSION=\"${SHFS_VERSION}\"

LINKER    := gcc
LDFLAGS    = 
LOADLIBES := 

all: ${ALL_TARGETS}

%.h: %.in
	sed -e '/^[ ]*#/d;/^$$/d;s/\\/\\\\/g;s/\"/\\\"/g;s/^\(.*\)$$/\"\1\\n\"/' <$< | sed -e "s/'/'\\\\\\\\''/g" >$@

proto.o: proto.c shell-test.h shell-code.h perl-test.h perl-code.h shfsmount.h

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

shfsmount: shfsmount.o proto.o
	${LINKER} ${LDFLAGS} -o $@ $+ ${LOADLIBES}

shfsumount: shfsumount.o
	${LINKER} ${LDFLAGS} -o $@ $+ ${LOADLIBES}
	
tidy:
	@${RM} core shfsmount.o shfsumount.o proto.o shell-test.h shell-code.h perl-test.h perl-code.h

clean: tidy
	@${RM} ${ALL_TARGETS}


install: shfsmount shfsumount
	install -m755 -b -D shfsmount ${ROOT}${SHFSMOUNT}
	install -m755 -b -D shfsumount ${ROOT}${SHFSUMOUNT}
	if [ ! -d ${ROOT}/sbin ]; then mkdir ${ROOT}/sbin; fi
	ln -fs ${SHFSMOUNT} ${ROOT}/sbin/mount.shfs

uninstall:
	rm -f ${ROOT}${SHFSMOUNT} ${ROOT}${SHFSUMOUNT} ${ROOT}/sbin/mount.shfs

.PHONY : all tidy clean install uninstall
