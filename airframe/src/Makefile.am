## Process this file with automake to produce Makefile.in

## ------------------------------------------------------------------------
## Makefile.am (source)
## autotools build system for Airframe
## ------------------------------------------------------------------------
## Copyright (C) 2006 Carnegie Mellon University. All Rights Reserved.
## ------------------------------------------------------------------------
## Authors: Brian Trammell 
## ------------------------------------------------------------------------
## GNU Lesser GPL Rights pursuant to Version 2.1, February 1999
## Government Purpose License Rights (GPLR) pursuant to DFARS 252.227-7013
## ------------------------------------------------------------------------

lib_LTLIBRARIES = libairframe.la
bin_PROGRAMS = airdaemon filedaemon

libairframe_la_SOURCES = airutil.c daeconfig.c logconfig.c privconfig.c \
        mio.c mio_stdio.c mio_source_file.c mio_sink_file.c \
        mio_common_net.c mio_source_udp.c mio_sink_udp.c \
        mio_source_tcp.c mio_sink_tcp.c \
        mio_source_pcap.c mio_sink_multi.c mio_config.c \
        airopt.c airlock.c
libairframe_la_LIBADD = @GLIB_LDADD@
libairframe_la_LDFLAGS = @GLIB_LIBS@ -version-info @LIBCOMPAT@ \
    -release ${VERSION}
libairframe_la_CFLAGS = @GLIB_CFLAGS@ 

airdaemon_SOURCES = airdaemon.c
airdaemon_LDADD = @GLIB_LDADD@ libairframe.la
airdaemon_LDFLAGS = @GLIB_LIBS@
airdaemon_CFLAGS = @GLIB_CFLAGS@

filedaemon_SOURCES = filedaemon.c
filedaemon_LDADD = @GLIB_LDADD@ libairframe.la
filedaemon_LDFLAGS = @GLIB_LIBS@
filedaemon_CFLAGS = @GLIB_CFLAGS@

noinst_HEADERS = mio_internal.h mio_common_net.h
