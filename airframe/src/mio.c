/*
** mio.c
** Multiple I/O configuration and routing support for file and network daemons
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
#include <airframe/mio.h>
#include <airframe/daeconfig.h>

#define MIOD_ERR1 {             \
        ok = FALSE;             \
        goto afterproc;         \
    }

#define MIOD_ERR2 {                                                             \
        ok = FALSE;                                                             \
        if (err && *err) {                                                      \
            xem = g_string_new(*err ? (*err)->message : "[null error]");        \
            g_clear_error(err);                                                 \
            g_string_append_printf(xem, "%s\n",                                 \
                                   ierr ? ierr->message : "[null error]");      \
            g_clear_error(&ierr);                                               \
        } else if (!xem) {                                                      \
            g_propagate_error(err, ierr);                                       \
            g_clear_error(&ierr);                                               \
        } else {                                                                \
            g_string_append_printf(xem, "%s\n",                                 \
                                   ierr ? ierr->message : "[null error]");      \
            g_clear_error(&ierr);                                               \
        }                                                                       \
    }

#define MIOD_ERR3 {                                                             \
        g_warning("%s", (err && err->message) ? err->message : "[null error]"); \
        g_clear_error(&err);                                                    \
    }

gboolean                mio_dispatch(
    MIOSource                *source,
    MIOSink                  *sink,
    MIOAppDriver             *app_drv,
    void                     *vctx,
    uint32_t                 *flags,
    GError                  **err)
{
    gboolean                 ok   = TRUE;
    GString                 *xem  = NULL;
    GError                  *ierr = NULL;

    /* clear MIO control flags */
    *flags &= ~MIO_F_CTL_MASK;

    /* check for termination */
    if (daec_did_quit()) {
        *flags |= MIO_F_CTL_TERMINATE;
        goto afterproc;
    }

    /* ensure available active source */
    if (!source->active) {
        /* get next source */
        if (source->next_source
            && !source->next_source(source, flags, err))
            MIOD_ERR1;
        source->opened = TRUE;
        if (app_drv->app_open_source
            && !app_drv->app_open_source(source, vctx, flags, err))
            MIOD_ERR1;
        source->active = TRUE;
    }

    /* ensure available active sink */
    if (!sink->active) {
        if (sink->next_sink
            && !sink->next_sink(source, sink, flags, err))
            MIOD_ERR1;
        sink->opened = TRUE;
        if (app_drv->app_open_sink
            && !app_drv->app_open_sink(source, sink, vctx, flags, err))
            MIOD_ERR1;
        sink->active = TRUE;
    }

    /* process an item */
    if (!app_drv->app_process(source, sink, vctx, flags, err))
        MIOD_ERR1;

  afterproc:
    /* promote poll to terminate if we're not a daemon. */
    if (*flags & MIO_F_CTL_POLL && !(*flags & MIO_F_OPT_DAEMON)) {
        *flags &= ~MIO_F_CTL_POLL;
        *flags |= MIO_F_CTL_TERMINATE;
    }

    /* close sink if closing source and source and sink are linked. */
    if (*flags & MIO_F_CTL_SOURCECLOSE && *flags & MIO_F_OPT_SINKLINK) {
        *flags |= MIO_F_CTL_SINKCLOSE;
    }

    /* close everything if quitting */
    if (*flags & MIO_F_CTL_TERMINATE) {
        *flags |= (MIO_F_CTL_SOURCECLOSE | MIO_F_CTL_SINKCLOSE);
    }

    /* close sink if necessary */
    if (*flags & MIO_F_CTL_SINKCLOSE) {
        if (sink->active) {
            sink->active = FALSE;
            if (app_drv->app_close_sink
                && !app_drv->app_close_sink(source, sink, vctx, flags, &ierr))
                MIOD_ERR2;
        }
        if (sink->opened) {
            sink->opened = FALSE;
            if (sink->close_sink
                && !sink->close_sink(source, sink, flags, &ierr))
                MIOD_ERR2;
        }
    }

    /* close source if necessary */
    if (*flags & MIO_F_CTL_SOURCECLOSE) {
        if (source->active) {
            source->active = FALSE;
            if (app_drv->app_close_source
                && !app_drv->app_close_source(source, vctx, flags, &ierr))
                MIOD_ERR2;
        }
        if (source->opened) {
            source->opened = FALSE;
            if (source->close_source
                && !source->close_source(source, flags, &ierr))
                MIOD_ERR2;
        }
    }

    /* done with this guy... */
    if (xem) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_MULTIPLE,
                    "Multiple errors in MIO dispatch: %s", xem->str);
        g_string_free(xem, TRUE);
    }
    return ok;
}

gboolean                    mio_dispatch_loop(
    MIOSource               *source,
    MIOSink                 *sink,
    MIOAppDriver            *app_drv,
    void                    *vctx,
    uint32_t                 flags,
    uint32_t                 polltime,
    uint32_t                 retrybase,
    uint32_t                 retrymax)
{
    uint32_t                 retrytime = retrybase;
    GError                  *err       = NULL;
    gboolean                 rv        = TRUE;

    while (1) {
        /* process a record */
        if (mio_dispatch(source, sink, app_drv, vctx, &flags, &err)) {
            /* success. reset retry delay. */
            retrytime = retrybase;
        } else {
            /* processing error. display error message if necessary. */
            if (flags & (MIO_F_CTL_ERROR | MIO_F_CTL_TRANSIENT)) {
                MIOD_ERR3;
                rv = FALSE;
            } else {
                g_clear_error(&err);
            }

            /* sleep if necessary */
            if (flags & MIO_F_CTL_TRANSIENT) {
                /* Transient error. Set retry delay. */
                sleep(retrytime);
                retrytime *= 2;
                if (retrytime > retrymax) retrytime = retrymax;
            } else if (flags & MIO_F_CTL_POLL) {
                /* No input. Set poll delay. */
                if (polltime) sleep(polltime);
            }
        }

        /* check for termination flag no matter what */
        if (flags & MIO_F_CTL_TERMINATE) break;
    }
    return rv;
}

void mio_source_free(
    MIOSource       *source)
{
    source->free_source(source);
}

void mio_sink_free(
    MIOSink         *sink)
{
    sink->free_sink(sink);
}

static void mio_source_free_app(
    MIOSource       *source)
{
    if (source->spec) g_free(source->spec);
}

gboolean mio_source_init_app(
    MIOSource        *source,
    const char       *spec,
    MIOType           vsp_type,
    void             *cfg,
    GError          **err)
{
    /* choose default type */
    if (vsp_type == MIO_T_ANY) vsp_type = MIO_T_APP;
    if (vsp_type != MIO_T_APP) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Cannot open application-specific source: type mismatch");
        return FALSE;
    }

    /* initialize source */
    source->spec         = g_strdup(spec);
    source->name         = source->spec;
    source->vsp_type     = vsp_type;
    source->cfg          = cfg;
    source->ctx          = NULL;
    source->next_source  = NULL;
    source->close_source = NULL;
    source->free_source  = mio_source_free_app;
    source->opened       = FALSE;
    source->active       = FALSE;

    return TRUE;
}

static void mio_sink_free_app(
    MIOSink         *sink)
{
    if (sink->spec) g_free(sink->spec);
}

gboolean mio_sink_init_app(
    MIOSink          *sink,
    const char       *spec,
    MIOType           vsp_type,
    void             *cfg,
    GError          **err)
{
    /* choose default type */
    if (vsp_type == MIO_T_ANY) vsp_type = MIO_T_APP;
    if (vsp_type != MIO_T_APP) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Cannot open application-specific sink: type mismatch");
        return FALSE;
    }

    /* initialize sink */
    sink->spec       = g_strdup(spec);
    sink->name       = sink->spec;
    sink->vsp_type   = vsp_type;
    sink->cfg        = cfg;
    sink->ctx        = NULL;
    sink->next_sink  = NULL;
    sink->close_sink = NULL;
    sink->free_sink  = mio_sink_free_app;
    sink->opened     = FALSE;
    sink->active     = FALSE;
    sink->iterative  = FALSE;

    return TRUE;
}
