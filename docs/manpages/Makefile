MANPATH=${ROOT}${MAN_PAGE_DIR}

all:

install:
ifneq (${MAN_PAGE_DIR},)
	install -m644 -b -D shfsmount.8 ${MANPATH}/man8/shfsmount.8
	install -m644 -b -D shfsumount.8 ${MANPATH}/man8/shfsumount.8
endif

uninstall:
ifneq (${MAN_PAGE_DIR},)
	rm -f ${MANPATH}/man8/shfsmount.8 ${MANPATH}/man8/shfsumount.8
endif

.PHONY : html html-clean all clean install uninstall
