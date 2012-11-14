/*
 ** mio_source_tcp.c
 ** Multiple I/O passive TCP single-client stream source
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2006-2011 Carnegie Mellon University. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell
 ** ------------------------------------------------------------------------
 ** @OPENSOURCE_HEADER_START@
 ** Use of the YAF system and related source code is subject to the terms 
 ** of the following licenses:
 ** 
 ** GNU Public License (GPL) Rights pursuant to Version 2, June 1991
 ** Government Purpose License Rights (GPLR) pursuant to DFARS 252.227.7013
 ** 
 ** NO WARRANTY
 ** 
 ** ANY INFORMATION, MATERIALS, SERVICES, INTELLECTUAL PROPERTY OR OTHER 
 ** PROPERTY OR RIGHTS GRANTED OR PROVIDED BY CARNEGIE MELLON UNIVERSITY 
 ** PURSUANT TO THIS LICENSE (HEREINAFTER THE "DELIVERABLES") ARE ON AN 
 ** "AS-IS" BASIS. CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY 
 ** KIND, EITHER EXPRESS OR IMPLIED AS TO ANY MATTER INCLUDING, BUT NOT 
 ** LIMITED TO, WARRANTY OF FITNESS FOR A PARTICULAR PURPOSE, 
 ** MERCHANTABILITY, INFORMATIONAL CONTENT, NONINFRINGEMENT, OR ERROR-FREE 
 ** OPERATION. CARNEGIE MELLON UNIVERSITY SHALL NOT BE LIABLE FOR INDIRECT, 
 ** SPECIAL OR CONSEQUENTIAL DAMAGES, SUCH AS LOSS OF PROFITS OR INABILITY 
 ** TO USE SAID INTELLECTUAL PROPERTY, UNDER THIS LICENSE, REGARDLESS OF 
 ** WHETHER SUCH PARTY WAS AWARE OF THE POSSIBILITY OF SUCH DAMAGES. 
 ** LICENSEE AGREES THAT IT WILL NOT MAKE ANY WARRANTY ON BEHALF OF 
 ** CARNEGIE MELLON UNIVERSITY, EXPRESS OR IMPLIED, TO ANY PERSON 
 ** CONCERNING THE APPLICATION OF OR THE RESULTS TO BE OBTAINED WITH THE 
 ** DELIVERABLES UNDER THIS LICENSE.
 ** 
 ** Licensee hereby agrees to defend, indemnify, and hold harmless Carnegie 
 ** Mellon University, its trustees, officers, employees, and agents from 
 ** all claims or demands made against them (and any related losses, 
 ** expenses, or attorney's fees) arising out of, or relating to Licensee's 
 ** and/or its sub licensees' negligent use or willful misuse of or 
 ** negligent conduct or willful misconduct regarding the Software, 
 ** facilities, or other rights or assistance granted by Carnegie Mellon 
 ** University under this License, including, but not limited to, any 
 ** claims of product liability, personal injury, death, damage to 
 ** property, or violation of any laws or regulations.
 ** 
 ** Carnegie Mellon University Software Engineering Institute authored 
 ** documents are sponsored by the U.S. Department of Defense under 
 ** Contract FA8721-05-C-0003. Carnegie Mellon University retains 
 ** copyrights in all material produced under this contract. The U.S. 
 ** Government retains a non-exclusive, royalty-free license to publish or 
 ** reproduce these documents, or allow others to do so, for U.S. 
 ** Government purposes only pursuant to the copyright license under the 
 ** contract clause at 252.227.7013.
 ** 
 ** @OPENSOURCE_HEADER_END@    
 ** ------------------------------------------------------------------------
 */
 
#define _AIRFRAME_SOURCE_
#include <airframe/mio_source_tcp.h>
#include "mio_common_net.h"

typedef struct _MIOSourceTCPContext {
    struct addrinfo         *ai;
    union {
        struct sockaddr_in  in4;
#if HAVE_GETADDRINFO
        struct sockaddr_in6 in6;
#endif
    }                       sa;
    uint32_t                sa_len;
    int                     lsock;
} MIOSourceTCPContext;

static gboolean mio_source_next_tcp(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    MIOSourceTCPContext     *tcpx = (MIOSourceTCPContext *)source->ctx;
    MIOSourceTCPConfig      *tcpc = (MIOSourceTCPConfig *)source->cfg;
    struct addrinfo         *ai = NULL;
    int                     sock, srv;
    fd_set                  lfdset;
    
    /* Create listening socket if necessary */
    if (tcpx->lsock < 0) {
        ai = tcpx->ai;
        do {
            tcpx->lsock = socket(ai->ai_family, ai->ai_socktype, 
                                 ai->ai_protocol);
            if (tcpx->lsock < 0) continue;
            if (bind(tcpx->lsock, ai->ai_addr, ai->ai_addrlen) < 0) {
                close(tcpx->lsock); tcpx->lsock = -1; continue;
            }
            if (listen(tcpx->lsock, 1) < 0) {
                close(tcpx->lsock); tcpx->lsock = -1; continue;
            }
            break;
        } while ((ai = ai->ai_next));

        /* check for no listenable socket */
        if (ai == NULL) {
            *flags |= MIO_F_CTL_ERROR;
            g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_CONN,
                        "couldn't create TCP socket listening to %s: %s", 
                        source->spec ? source->spec : "default", 
                        strerror(errno));
            return FALSE;
        }        
    }
    
    /* accept connection without blocking */
    FD_ZERO(&lfdset);
    FD_SET(tcpx->lsock, &lfdset);
    srv = select(tcpx->lsock + 1, &lfdset, NULL, NULL, &(tcpc->timeout));
    if (srv < 0) {
        if (errno == EINTR) {
            g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_NOINPUT,
                        "Interrupted select");
            return FALSE;            
        } else {
            *flags |= MIO_F_CTL_ERROR;
            g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_IO,
                        "error waiting for a TCP connection on %s: %s", 
                        source->spec ? source->spec : "default", 
                        strerror(errno));
            return FALSE;
        }
    } else if (srv == 0) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_NOINPUT,
                    "No connections waiting");
        return FALSE;
    } 
    
    /* Kickass. We have an acceptable socket. */
    g_assert(FD_ISSET(tcpx->lsock, &lfdset));    
    tcpx->sa_len = sizeof(tcpx->sa);
    sock = accept(tcpx->lsock, (struct sockaddr *)&(tcpx->sa), &(tcpx->sa_len));
    if (sock < 0) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_IO,
                    "error accepting a TCP connection on %s: %s",
                    source->spec ? source->spec : "default", strerror(errno));
        return FALSE;
    }
    
    /* Store file descriptor */
    source->vsp = GINT_TO_POINTER(sock);
    
    return TRUE;
}

static gboolean mio_source_close_tcp(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    close(GPOINTER_TO_INT(source->vsp));
    source->vsp = GINT_TO_POINTER(-1);
    return TRUE;
}

static void mio_source_free_tcp(
    MIOSource               *source)
{
    MIOSourceTCPContext     *tcpx = (MIOSourceTCPContext *)source->ctx;

    if (source->spec) g_free(source->spec);
    if (source->name) g_free(source->name);
    if (tcpx) {
        mio_freeaddrinfo((struct addrinfo *)tcpx->ai);
        close(GPOINTER_TO_UINT(tcpx->lsock));
        g_free(tcpx);
    }
}

gboolean mio_source_init_tcp(
    MIOSource           *source,
    const char          *spec,
    MIOType             vsp_type,
    void                *cfg,
    GError              **err)
{
    MIOSourceTCPContext *tcpx = NULL;
    MIOSourceTCPConfig  *tcpc = (MIOSourceTCPConfig *)cfg;
    gboolean            ok = TRUE;
    char                *splitspec = NULL, *hostaddr = NULL, *svcaddr = NULL;

    /* choose default type */
    if (vsp_type == MIO_T_ANY) vsp_type = MIO_T_SOCK_STREAM;
    
    /* initialize TCP source */
    source->spec = spec ? g_strdup(spec) : NULL;
    source->name = NULL;
    source->vsp_type = vsp_type;
    source->vsp = NULL;
    source->ctx = NULL;
    source->cfg = cfg;
    source->next_source = mio_source_next_tcp;
    source->close_source = mio_source_close_tcp;
    source->free_source = mio_source_free_tcp;
    source->opened = FALSE;
    source->active = FALSE;
    
    /* Ensure type is valid */
    if (vsp_type != MIO_T_SOCK_STREAM) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Cannot create TCP source: type mismatch");
        ok = FALSE;        
        goto end;
    }
    
    /* Parse specifier */
    splitspec = spec ? g_strdup(spec) : NULL;
    mio_init_ip_splitspec(splitspec, TRUE, tcpc->default_port, 
                          &hostaddr, &svcaddr, &source->name);
    
    /* Create context */
    tcpx = g_new0(MIOSourceTCPContext, 1);
    
    /* Do lookup */
    if (!(tcpx->ai = mio_init_ip_lookup(hostaddr, svcaddr, 
                                        SOCK_STREAM, IPPROTO_TCP, TRUE, err))) {
        ok = FALSE; 
        goto end;
    }
    
    /* force listening socket creation */
    tcpx->lsock = -1;
    
    /* stash the context */
    source->ctx = tcpx;
        
 end:
    if (!ok && tcpx) {
        if (tcpx->ai) mio_freeaddrinfo(tcpx->ai);
        g_free(tcpx);
    }
    if (splitspec) g_free(splitspec);
    
    return ok;
}
