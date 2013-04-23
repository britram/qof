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


PODFILES = yaf.pod
man1_MANS = yaf.1
HTMLFILES = yaf.html

WARN_CFLAGS = -g -DDEBUG -Wall -W -Wmissing-prototypes -Wformat=2 -Wdeclaration-after-statement -Wpointer-arith -Wshadow

.pod.1:
	$(POD2MAN) $(YAF_POD2MAN_ARGS) --section=1 $< $@

.pod.html:
	$(POD2HTML) --noindex --infile="$<" --outfile="$@"
	$(top_srcdir)/doc/add-header.pl "$@"

copy-man-pages: $(HTMLFILES)
	mkdir -p $(top_builddir)/doc/html
	cp $(HTMLFILES) $(top_builddir)/doc/html

docs: $(man1_MANS) copy-man-pages

clean-docs:
	rm -f $(man1_MANS) $(HTMLFILES)

CLEANFILES = $(man1_MANS) $(HTMLFILES)

EXTRA_DIST = $(PODFILES)

clean-local:
	rm -f pod2htm*.tmp

lib_LTLIBRARIES = libqof.la
bin_PROGRAMS = qof

INCLUDES = $(LTDLINCL)
LIBS += $(LIBLTDL)

libqof_la_SOURCES = yafcore.c yaftab.c yafrag.c decode.c picq.c ring.c bitmap.c streamstat.c qofdyn.c qofifmap.c

if CYGWIN
libqof_la_SOURCES += yafcygwin.c
endif

libqof_la_LIBADD = @GLIB_LDADD@
libqof_la_LDFLAGS = @GLIB_LIBS@ @libfixbuf_LIBS@ -version-info @LIBCOMPAT@ -release ${VERSION}
libqof_la_CFLAGS = @GLIB_CFLAGS@ @libfixbuf_CFLAGS@ -DYAF_CONF_DIR='"$(sysconfdir)"'

qof_SOURCES = qof.c yafstat.c yafcap.c qofltrace.c yafout.c yaflush.c qofconfig.c
qof_LDADD   =  libqof.la @GLIB_LDADD@
qof_LDFLAGS = -L../airframe/src -lairframe @GLIB_LIBS@ @libfixbuf_LIBS@ -export-dynamic
qof_CFLAGS  = @GLIB_CFLAGS@ @libfixbuf_CFLAGS@ -DYAF_CONF_DIR='"$(sysconfdir)"'

noinst_HEADERS = yafcap.h qofltrace.h yafstat.h yafout.h yaflush.h qofconfig.h qofifmap.h

