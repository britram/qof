/*
 ** mio_sink_multi.c
 ** Multiple I/O compound sink, for output fanout case
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2006-2011 Carnegie Mellon University. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell
 ** ------------------------------------------------------------------------
 ** @OPENSOURCE_HEADER_START@K
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
#include <airframe/mio_sink_multi.h>

static gboolean mio_sink_next_multi(
    MIOSource       *source,
    MIOSink         *sink,
    uint32_t        *flags,
    GError          **err)
{
    MIOSink         *ms = NULL, *cs = NULL;
    GError          *err2 = NULL;
    uint32_t        i, j;
    
    for (i = 0; i < mio_smc(sink); i++) {
        ms = &mio_smn(sink, i);
        if (!ms->next_sink(source, ms, flags, err)) {
            /* on error, close all sinks that already went next. */
            for (j = 0; j < i; j++) {
                cs = &mio_smn(sink, j);
                if (!cs->close_sink(source, cs, flags, &err2)) {
                    /* error closing an opened sink... bail for now */
                    g_error("panic on multiple sink next: "
                            "couldn't close sink %s: %s on error "
                            "while opening sink %s: %s", 
                            cs->spec, err2->message, 
                            ms->spec, (*err)->message);
                }
            }
            
            /* all sinks opened by this operation closed. */
            return FALSE;
        }
    }

    /* done. */
    return TRUE;
}

static gboolean mio_sink_close_multi(
    MIOSource       *source,
    MIOSink         *sink,
    uint32_t        *flags,
    GError          **err)
{
    GString         *errstr = NULL;
    GError          *err2 = NULL;
    uint32_t        errcount = 0;
    MIOSink         *ms = NULL;
    uint32_t        i;
    
    /* close subordinate sinks */
    for (i = 0; i < mio_smc(sink); i++) {
        ms = &mio_smn(sink, i);
        if (!ms->close_sink(source, ms, flags, &err2)) {
            if (!errstr) errstr = g_string_new("");
            g_string_append_printf(errstr, "%s\n", err2->message);
            errcount++;
            g_clear_error(&err2);
        }
    }
    
    /* report errors */
    if (errcount) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_MULTIPLE, "%u error(s) closing sink: %s", errcount, errstr->str);
        g_string_free(errstr, TRUE);
        return FALSE;
    }
    
    /* done */
    return TRUE;
}

static void mio_sink_free_multi(
    MIOSink                 *sink)
{
    MIOSink         *ms = NULL;
    uint32_t        i;

    for (i = 0; i < mio_smc(sink); i++) {
        ms = &mio_smn(sink, i);
        ms->free_sink(ms);
    }

    if (sink->spec) g_free(sink->spec);
    if (sink->vsp) g_free(sink->vsp);
}

gboolean mio_sink_init_multi(
    MIOSink         *sink,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{
    uint32_t        vsp_count = GPOINTER_TO_UINT(cfg);

    /* choose default type */
    if (vsp_type == MIO_T_ANY) vsp_type = MIO_T_SINKARRAY;

    /* Ensure type is valid */
    if (vsp_type != MIO_T_SINKARRAY) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Cannot create multiple sink: type mismatch");
        return FALSE;
    }
    
    /* Ensure array length is valid */
    if (!vsp_count) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Cannot create multiple sink: zero sinks");
        return FALSE;
    }
    
    /* initialize multi sink */
    if (spec) {
        sink->spec = g_strdup(spec);
    } else {
        sink->spec = NULL;
    }
    sink->name = NULL;
    sink->vsp_type = vsp_type;
    sink->vsp = g_new0(MIOSink, vsp_count);
    sink->ctx = NULL;
    sink->cfg = cfg;
    sink->next_sink = mio_sink_next_multi;
    sink->close_sink = mio_sink_close_multi;
    sink->free_sink = mio_sink_free_multi;
    sink->opened = FALSE;
    sink->active = FALSE;
    sink->iterative = TRUE;
    
    return TRUE;
}
