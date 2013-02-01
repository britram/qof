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


PODFILES = yaf.pod yafscii.pod
man1_MANS = yaf.1 yafscii.1
HTMLFILES = yaf.html yafscii.html

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

lib_LTLIBRARIES = libyaf.la
bin_PROGRAMS = yaf yafscii yafcollect

INCLUDES = $(LTDLINCL)
LIBS += $(LIBLTDL)

libyaf_la_SOURCES = yafcore.c yaftab.c yafrag.c decode.c picq.c ring.c 

if PLUGINENABLE
libyaf_la_SOURCES += yafhooks.c
endif

if CYGWIN
libyaf_la_SOURCES += yafcygwin.c
endif

libyaf_la_LIBADD = @GLIB_LDADD@ 
libyaf_la_LDFLAGS = @GLIB_LIBS@ @libfixbuf_LIBS@-version-info @LIBCOMPAT@ -release ${VERSION}
libyaf_la_CFLAGS = @GLIB_CFLAGS@ @libfixbuf_CFLAGS@ -DYAF_CONF_DIR='"$(sysconfdir)"'

yaf_SOURCES = yaf.c yafstat.c yafdag.c yafcap.c yafout.c yaflush.c yafpcapx.c
yaf_LDADD   =  libyaf.la @GLIB_LDADD@
yaf_LDFLAGS = -L../airframe/src -lairframe @GLIB_LIBS@ @libfixbuf_LIBS@ -export-dynamic
yaf_CFLAGS  = @GLIB_CFLAGS@ @libfixbuf_CFLAGS@  -DYAF_CONF_DIR='"$(sysconfdir)"'

yafscii_SOURCES = yafscii.c
yafscii_LDADD = libyaf.la @GLIB_LDADD@ 
yafscii_LDFLAGS =  -L../airframe/src -lairframe @GLIB_LIBS@ @libfixbuf_LIBS@
yafscii_CFLAGS = @GLIB_CFLAGS@ @libfixbuf_CFLAGS@ 

yafcollect_SOURCES = yafcollect.c
yafcollect_LDADD = libyaf.la @GLIB_LDADD@
yafcollect_LDFLAGS =  -L../airframe/src -lairframe @GLIB_LIBS@ @libfixbuf_LIBS@
yafcollect_CFLAGS = @GLIB_CFLAGS@ @libfixbuf_CFLAGS@

noinst_HEADERS = yafdag.h yafcap.h yafpcapx.h yafstat.h yafout.h yaflush.h qofctx.h

