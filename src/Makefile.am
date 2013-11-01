## Process this file with automake to produce Makefile.in
## ------------------------------------------------------------------------
## Makefile.am (source)
## autotools build system for YAF
## ------------------------------------------------------------------------
## Copyright (C) 2006-2012 Carnegie Mellon University. All Rights Reserved.
## ------------------------------------------------------------------------
## Authors: Brian Trammell 
## ------------------------------------------------------------------------
## GNU General Public License (GPL) Rights pursuant to Version 2, June 1991
## Government Purpose License Rights (GPLR) pursuant to DFARS 252.227-7013
## ------------------------------------------------------------------------


PODFILES = qof.pod
man1_MANS = qof.1
HTMLFILES = qof.html
PDFFILES = qof.pdf

WARN_CFLAGS = -g -DDEBUG -Wall -W -Wmissing-prototypes -Wformat=2 -Wdeclaration-after-statement -Wpointer-arith -Wshadow

.pod.1:
	$(POD2MAN) $(YAF_POD2MAN_ARGS) --section=1 $< $@

.pod.html:
	$(POD2HTML) --noindex --infile="$<" --outfile="$@"

.1.pdf:
	groff -mandoc $< | ps2pdf - $@

copy-man-pages: $(HTMLFILES)
	mkdir -p $(top_builddir)/doc/html
	cp $(HTMLFILES) $(top_builddir)/doc/html
	cp $(PDFFILES) $(top_builddir)/doc

docs: $(man1_MANS) $(PDFFILES) copy-man-pages

clean-docs:
	rm -f $(man1_MANS) $(HTMLFILES)

CLEANFILES = $(man1_MANS) $(HTMLFILES)

EXTRA_DIST = $(PODFILES)

clean-local:
	rm -f pod2htm*.tmp

lib_LTLIBRARIES = libqof.la
bin_PROGRAMS = qof

libqof_la_SOURCES = yafcore.c yaftab.c yafrag.c decode.c picq.c ring.c \
                    bitmap.c streamstat.c qofifmap.c qofmaclist.c \
                    qofseq.c qofack.c qofrtt.c qofrwin.c qofopt.c

libqof_la_LIBADD = @GLIB_LDADD@
libqof_la_LDFLAGS = @GLIB_LIBS@ @libfixbuf_LIBS@ -version-info @LIBCOMPAT@ -release ${VERSION}
libqof_la_CFLAGS = @GLIB_CFLAGS@ @libfixbuf_CFLAGS@ -DYAF_CONF_DIR='"$(sysconfdir)"'

qof_SOURCES = qof.c yafstat.c qofltrace.c yafout.c yaflush.c qofconfig.c qofdetune.c
qof_LDADD   =  libqof.la @GLIB_LDADD@
qof_LDFLAGS = -L../airframe/src -lairframe @GLIB_LIBS@ @libfixbuf_LIBS@ -export-dynamic
qof_CFLAGS  = @GLIB_CFLAGS@ @libfixbuf_CFLAGS@ -DYAF_CONF_DIR='"$(sysconfdir)"'

noinst_HEADERS = qofltrace.h yafstat.h yafout.h yaflush.h qofconfig.h qofdetune.h

