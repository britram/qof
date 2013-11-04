/*
 ** yaflush.c
 ** YAF unified flow/flush logic
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2006-2012 Carnegie Mellon University. All Rights Reserved.
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

#define _YAF_SOURCE_
#include "yaflush.h"
#include "yafout.h"
#include "yafstat.h"
#include <qof/yafcore.h>

gboolean yfProcessPBufRing(
    qfContext_t        *ctx,
    GError             **err)
{
    AirLock             *lock = NULL;
    yfPBuf_t            *pbuf = NULL;
    gboolean            ok = TRUE;
    uint64_t            cur_time;

    /* point to lock buffer if we need it */
    if (ctx->octx.enable_lock) {
        lock = &ctx->octx.lockbuf;
    }

    /* Open output if we need to */
    if (!ctx->octx.fbuf) {
        if (!(ctx->octx.fbuf = yfOutputOpen(&ctx->octx, lock, err))) {
            ok = FALSE;
            goto end;
        }
    }

    /* Dump statistics if requested */
    yfStatDumpLoop();

    /* process packets from the ring buffer */
    while ((pbuf = (yfPBuf_t *)rgaNextTail(ctx->pbufring))) {

        /* Skip time zero packets (these are marked invalid) */
        if (!pbuf->ptime) {
            continue;
        }

        /* Add the packet to the flow table */
        yfFlowPBuf(ctx->flowtab, pbuf);
    }

    /* Flush the flow table */
    if (!yfFlowTabFlush(ctx, FALSE, err)) {
        ok = FALSE;
        goto end;
    }

    /* Close output file for rotation if necessary */
    if (ctx->octx.rotate_period) {
        cur_time = yfFlowTabCurrentTime(ctx->flowtab);
        if (ctx->octx.rotate_last) {
            if (cur_time - ctx->octx.rotate_last > ctx->octx.rotate_period) {
                yfOutputClose(ctx->octx.fbuf, lock, TRUE);
                ctx->octx.fbuf = NULL;
                ctx->octx.rotate_last = cur_time;
                if (!(ctx->octx.fbuf = yfOutputOpen(&ctx->octx, lock, err))) {
                    ok = FALSE;
                    goto end;
                }
            }
        } else {
            ctx->octx.rotate_last = cur_time;
        }
    }

end:
    return ok;
}

gboolean yfTimeOutFlush(
    qfContext_t        *ctx,
    GError             **err)
{
    AirLock             *lock = NULL;
    uint64_t            ctime;

    /* point to lock buffer if we need it */
    if (ctx->octx.enable_lock) {
        lock = &ctx->octx.lockbuf;
    }

    /* Open output if we need to */
    if (!ctx->octx.fbuf) {
        if (!(ctx->octx.fbuf = yfOutputOpen(&ctx->octx, lock, err))) {
            return FALSE;
        }
    }

    /* Dump statistics if requested */
    yfStatDumpLoop();

    /* Flush the flow table */
    if (!yfFlowTabFlush(ctx, FALSE, err)) {
        return FALSE;
    }
    
    if (!fBufEmit(ctx->octx.fbuf, err)) {
        return FALSE;
    }

    /* Close output file for rotation if necessary */
    if (ctx->octx.rotate_period) {
        ctime = yfFlowTabCurrentTime(ctx->flowtab);
        if (ctx->octx.rotate_last) {
            if (ctime - ctx->octx.rotate_last > ctx->octx.rotate_period) {
                yfOutputClose(ctx->octx.fbuf, lock, TRUE);
                ctx->octx.fbuf = NULL;
                ctx->octx.rotate_last = ctime;
            }
        } else {
            ctx->octx.rotate_last = ctime;
        }
    }

    return TRUE;
}



gboolean yfFinalFlush(
    qfContext_t         *ctx,
    gboolean            ok,
    GError              **err)
{
    AirLock             *lock = NULL;
    gboolean            frv;
    gboolean            srv = TRUE;

    /* point to lock buffer if we need it */
     if (ctx->octx.enable_lock) {
         lock = &ctx->octx.lockbuf;
     }

    /* handle final flush and close */
    if (ctx->octx.fbuf) {
        if (ok) {
            /* Flush flow buffer and close output file on successful exit */
            frv = yfFlowTabFlush(ctx, TRUE, err);
            if (ctx->octx.stats_period) {
                srv = yfWriteStatsRec(ctx, err);
            }
            yfOutputClose(ctx->octx.fbuf, lock, TRUE);
            if (!frv || !srv) {
                ok = FALSE;
            }
        } else {
            /* Just close output file on error */
            yfOutputClose(ctx->octx.fbuf, lock, FALSE);
        }
    }

    return ok;
}
