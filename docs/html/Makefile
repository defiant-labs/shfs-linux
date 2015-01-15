HTMLPATH = ${ROOT}${HTML_DOC_DIR}

HTMLFILES = faq.html index.html install.html internals.html

all: ${HTMLFILES}

%.html: %.in linkbar
	@ echo $< -\> $@
	@ >$@; cat $<|while read i; do\
		echo $$i >>$@;\
		if echo $$i|grep '^<!-- linkbar -->$$' >/dev/null; then\
			cat linkbar >>$@;\
		fi\
	done

sf: all
	for i in ${HTMLFILES}; do\
		sed -e 's|../../\([^"]*\)|http://cvs.sourceforge.net/cgi-bin/viewcvs.cgi/shfs/shfs/\1?rev=HEAD|g' $$i|while read l; do\
			if echo $$l|grep '^<!-- logo -->$$' >/dev/null; then\
				cat logo >>$$i.tmp;\
			else\
				echo $$l >>$$i.tmp;\
			fi\
		done;\
		mv $$i.tmp $$i;\
	done

html-clean:
	rm -f *.html

clean:

install: all
ifneq (${HTML_DOC_DIR},)
	for i in *.html *.css images/*; do\
		install -m644 -b -D $$i ${HTMLPATH}/$$i;\
	done

endif

uninstall:
ifneq (${HTML_DOC_DIR},)
	rm -f ${HTMLPATH}/*.html ${HTMLPATH}/*.css ${HTMLPATH}/images/*
endif

.PHONY : sf all clean install uninstall
