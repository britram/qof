/*
 ** mio_sink_tcp.c
 ** Multiple I/O active TCP stream sink
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
#include <airframe/mio_sink_tcp.h>
#include "mio_common_net.h"

gboolean mio_sink_init_tcp(
    MIOSink         *sink,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{
    char            *splitspec = NULL, *hostaddr = NULL, *svcaddr = NULL;
    gboolean        ok = TRUE;
    
    /* choose default type */
    if (vsp_type == MIO_T_ANY) vsp_type = MIO_T_SOCK_STREAM;
    
    /* initialize UDP sink */
    sink->spec = g_strdup(spec);
    sink->name = NULL;
    sink->vsp_type = vsp_type;
    sink->vsp = NULL;
    sink->ctx = NULL;
    sink->cfg = cfg;
    sink->next_sink = mio_sink_next_common_net;
    sink->close_sink = mio_sink_close_common_net;
    sink->free_sink = mio_sink_free_common_net;
    sink->opened = FALSE;
    sink->active = FALSE;
    sink->iterative = FALSE;
    
    /* Ensure type is valid */
    if (vsp_type != MIO_T_SOCK_STREAM) {
        ok = FALSE;
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Cannot create TCP sink: type mismatch");
        goto end;
    }
    
    /* Parse specifier */
    splitspec = spec ? g_strdup(spec) : NULL;
    mio_init_ip_splitspec(splitspec, FALSE, (char *)cfg,
                          &hostaddr, &svcaddr, &(sink->name));

    /* Check for no host */
    if (!hostaddr) {
        ok = FALSE;
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Cannot create TCP sink: no output specifier");
        goto end;
    }
    
    /* Do lookup and create context */
    if (!(sink->ctx = mio_init_ip_lookup(hostaddr, svcaddr, SOCK_STREAM,
                                         IPPROTO_TCP, FALSE, err))) {
        ok = FALSE;
        goto end;
    }

end:
    if (splitspec) g_free(splitspec);    
    return ok;
}
