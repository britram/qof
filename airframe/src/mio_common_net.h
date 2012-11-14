/*
 ** mio_common_net.h
 ** Multiple I/O network source/sink common support and addrinfo glue
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2006-2011 Carnegie Mellon University. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell
 ** ------------------------------------------------------------------------
 ** GNU Lesser GPL Rights pursuant to Version 2.1, February 1999 
 ** Government Purpose License Rights (GPLR) pursuant to DFARS 252.227-7013
 ** ------------------------------------------------------------------------
 */

/* idem hack */
#ifndef _AIRFRAME_MIO_COMMON_NET_H_
#define _AIRFRAME_MIO_COMMON_NET_H_
#include <airframe/mio.h>

#ifndef HAVE_GETADDRINFO
struct addrinfo {
    int ai_family;              /* protocol family for socket */
    int ai_socktype;            /* socket type */
    int ai_protocol;            /* protocol for socket */
    socklen_t ai_addrlen;       /* length of socket-address */
    struct sockaddr *ai_addr;   /* socket-address for socket */
    struct addrinfo *ai_next;   /* pointer to next in list */
};
#endif

void mio_freeaddrinfo(
    struct addrinfo     *ai);

struct addrinfo *mio_init_ip_lookup(
    char                *hostaddr,
    char                *svcaddr,
    int                 socktype,
    int                 protocol,
    gboolean            passive,
    GError              **err);

void mio_init_ip_splitspec(
    char            *spec,
    gboolean        passive,
    char            *default_port,
    char            **hostaddr,
    char            **svcaddr,
    char            **srcname);

gboolean mio_sink_next_common_net(
    MIOSource               *source,
    MIOSink                 *sink,
    uint32_t                *flags,
    GError                  **err);

gboolean mio_sink_close_common_net(
    MIOSource               *source,
    MIOSink                 *sink,
    uint32_t                *flags,
    GError                  **err);
    
void mio_sink_free_common_net(
    MIOSink                 *sink);


/* end idem */
#endif
