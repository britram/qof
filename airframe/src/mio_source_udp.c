/*
 ** mio_source_udp.c
 ** Multiple I/O passive UDP datagram source
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
#include <airframe/mio_source_udp.h>
#include "mio_common_net.h"

static gboolean mio_source_next_udp(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    struct addrinfo         *ai = (struct addrinfo *)source->ctx;
    int                     sock;
    
    /* open a socket */
    do {
        sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sock < 0) continue;
        if (bind(sock, ai->ai_addr, ai->ai_addrlen) == 0) break;
        close(sock);
    } while ((ai = ai->ai_next));
    
    /* check for no openable socket */
    if (ai == NULL) {
        *flags |= MIO_F_CTL_ERROR;
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_CONN,
                    "couldn't create bound UDP socket to %s: %s", 
                    source->spec ? source->spec : "default", strerror(errno));
        return FALSE;
    }

    /* store file descriptor */
    source->vsp = GINT_TO_POINTER(sock);
    
    return TRUE;
}

static gboolean mio_source_close_udp(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    /* Close socket */
    close(GPOINTER_TO_INT(source->vsp));
    source->vsp = GINT_TO_POINTER(-1);
    
    /* All done */
    return TRUE;
}

static void mio_source_free_udp(
    MIOSource               *source)
{
    if (source->spec) g_free(source->spec);
    if (source->name) g_free(source->name);
    mio_freeaddrinfo((struct addrinfo *)source->ctx);
}

gboolean mio_source_init_udp(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{
    char            *splitspec = NULL, *hostaddr = NULL, *svcaddr = NULL;
    
    /* choose default type */
    if (vsp_type == MIO_T_ANY) vsp_type = MIO_T_SOCK_DGRAM;
    
    /* initialize UDP source */
    source->spec = spec ? g_strdup(spec) : NULL;
    source->name = NULL;
    source->vsp_type = vsp_type;
    source->vsp = NULL;
    source->ctx = NULL;
    source->cfg = cfg;
    source->next_source = mio_source_next_udp;
    source->close_source = mio_source_close_udp;
    source->free_source = mio_source_free_udp;
    source->opened = FALSE;
    source->active = FALSE;
    
    /* Ensure type is valid */
    if (vsp_type != MIO_T_SOCK_DGRAM) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Cannot create UDP source: type mismatch");
        return FALSE;        
    }
    
    /* Parse specifier */
    splitspec = spec ? g_strdup(spec) : NULL;
    mio_init_ip_splitspec(splitspec, TRUE, (char *)cfg, 
                          &hostaddr, &svcaddr, &source->name);
    
    
    /* Do lookup and create context */
    source->ctx = mio_init_ip_lookup(hostaddr, svcaddr, 
                                     SOCK_DGRAM, IPPROTO_UDP, TRUE, err);

    if (splitspec) g_free(splitspec);
    
    return source->ctx ? TRUE : FALSE;
}
