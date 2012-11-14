/*
 ** mio_stdio.c
 ** Multiple I/O standard in source / standard out sink
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
#include <airframe/mio_stdio.h>

gboolean mio_source_check_stdin(
    MIOSource       *source,
    uint32_t        *flags,
    GError          **err)
{
    /* terminate the application if standard input has been closed. */
    if (!source->name) {
        *flags |= MIO_F_CTL_TERMINATE;
        return FALSE;
    }
    
    return TRUE;
}

gboolean mio_source_close_stdin(
    MIOSource       *source,
    uint32_t        *flags,
    GError          **err)
{
    source->name = NULL;
    return TRUE;
}

gboolean mio_source_init_stdin(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{
    /* match spec */
    if (strcmp(spec, "-")) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Cannot open stdin source: spec mismatch");
        return FALSE;
    }

    /* choose default type */
    if (vsp_type == MIO_T_ANY) vsp_type = MIO_T_FP;

    /* initialize source */
    source->spec = "-";
    source->name = "-";
    source->vsp_type = vsp_type;
    source->cfg = NULL;
    source->ctx = NULL;
    source->next_source = mio_source_check_stdin;
    source->close_source = mio_source_close_stdin;
    source->free_source = NULL;
    source->opened = FALSE;
    source->active = FALSE;

    /* set up source pointer as appropriate */
    switch (vsp_type) {
    case MIO_T_NULL:
        source->vsp = NULL;
        break;
    case MIO_T_FD:
        source->vsp = GINT_TO_POINTER(0);
        break;
    case MIO_T_FP:
        source->vsp = stdin;
        break;
    default:
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Cannot open stdin source: type mismatch");
        return FALSE;
    }
    
    return TRUE;
}

gboolean mio_sink_init_stdout(
    MIOSink         *sink,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{
    /* match spec */
    if (strcmp(spec, "-")) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Cannot open stdout sink: spec mismatch");
        return FALSE;
    }
    
    /* choose default type */
    if (vsp_type == MIO_T_ANY) vsp_type = MIO_T_FP;

    /* initialize sink */
    sink->spec = "-";
    sink->name = "-";
    sink->vsp_type = vsp_type;
    sink->cfg = NULL;
    sink->ctx = NULL;
    sink->next_sink = NULL;
    sink->close_sink = NULL;
    sink->free_sink = NULL;
    sink->opened = FALSE;
    sink->active = FALSE;
    sink->iterative = FALSE;
    
    /* set up sink pointer as appropriate */
    switch (vsp_type) {
    case MIO_T_NULL:
        sink->vsp = NULL;
        break;
    case MIO_T_FD:
        sink->vsp = GINT_TO_POINTER(1);
        break;
    case MIO_T_FP:
        sink->vsp = stdout;
        break;
    default:
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Cannot open stdout sink: type mismatch");
        return FALSE;
    }
    
    return TRUE;
}
