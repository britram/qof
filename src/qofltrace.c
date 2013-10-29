/**
 * @internal
 *
 ** qofltrace.c
 ** QoF libtrace input support
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C)      2013 Brian Trammell.             All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell <brian@trammell.ch>
 ** ------------------------------------------------------------------------
 ** QoF is made available under the terms of the GNU Public License (GPL)
 ** Version 2, June 1991
 ** ------------------------------------------------------------------------
 */

#define _YAF_SOURCE_
#include <qof/autoinc.h>

#include <qof/yafcore.h>
#include <qof/yaftab.h>
#include <qof/decode.h>
#include "yafout.h"
#include "yaflush.h"
#include "yafstat.h"

#include "qofltrace.h"
#include "qofdetune.h"

#define TRACE_PACKET_GROUP 32

/* Quit flag support */
extern int yaf_quit;

/* Statistics */
static uint32_t            yaf_trace_drop = 0;

struct qfTraceSource_st {
    libtrace_t          *trace;
    libtrace_packet_t   *packet;
    libtrace_filter_t   *filter;
};

qfTraceSource_t *qfTraceOpen(const char *uri,
                             const char *bpf,
                             int snaplen,
                             GError **err)
{
    qfTraceSource_t *lts;
    libtrace_err_t  terr;
    
    lts = g_new0(qfTraceSource_t, 1);
    
    if (!(lts->packet = trace_create_packet())) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "Could not initialize libtrace packet");
        goto err;
    }
    
    lts->trace = trace_create(uri);
    if (trace_is_err(lts->trace)) {
        terr = trace_get_err(lts->trace);
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "Could not open libtrace URI %s: %s",
                    uri, terr.problem);
        goto err;
    }
    
    if (trace_config(lts->trace, TRACE_OPTION_SNAPLEN, &snaplen) == -1) {
        terr = trace_get_err(lts->trace);
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "Could not set snaplen on libtrace URI %s: %s",
                    uri, terr.problem);
        goto err;
    }
    
    if (bpf) {
        if (!(lts->filter = trace_create_filter(bpf))) {
            g_warning("Could not compile libtrace BPF %s", bpf);
            goto err;
        }
        
        if (trace_config(lts->trace, TRACE_OPTION_FILTER, lts->filter) == -1) {
            terr = trace_get_err(lts->trace);
            g_warning("Could not set libtrace filter: %s", terr.problem);
            goto err;
        }
    }
    
    /* start processing */
    if (trace_start(lts->trace) == -1) {
        terr = trace_get_err(lts->trace);
        g_warning("libtrace trace_start() error: %s", terr.problem);
        goto err;
    }

    
    return lts;
  
err:
    qfTraceClose(lts);
    return NULL;
}

void qfTraceClose(qfTraceSource_t *lts) {
    if (lts->filter) trace_destroy_filter(lts->filter);
    if (lts->packet) trace_destroy_packet(lts->packet);
    if (lts->trace) trace_destroy(lts->trace);
    if (lts) g_free(lts);
}


static void qfTraceHandle(qfTraceSource_t    *lts,
                                 qfContext_t        *ctx)
{
    yfPBuf_t                    *pbuf;
    yfIPFragInfo_t              fraginfo_buf,
    *fraginfo = ctx->fragtab ?
    &fraginfo_buf : NULL;
    libtrace_linktype_t         linktype;
    uint8_t                     *pkt;
    uint32_t                    caplen;
    
    struct timeval tv;
    
    /* get next spot in ring buffer */
    pbuf = (yfPBuf_t *)rgaNextHead(ctx->pbufring);
    g_assert(pbuf);
    
#if QOF_ENABLE_DETUNE
    do {
#endif
        /* extract data from libtrace */
        tv = trace_get_timeval(lts->packet);
        pkt = trace_get_packet_buffer(lts->packet, &linktype, &caplen);
    
        /* Decode packet into packet buffer */
        if (!yfDecodeToPBuf(ctx->dectx,
                            linktype,
                            yfDecodeTimeval(&tv),
                            trace_get_capture_length(lts->packet),
                            pkt, fraginfo, pbuf))
        {
            /* Couldn't decode packet; counted in dectx. Skip. */
            return;
        }
    
#if QOF_ENABLE_DETUNE
    /* Check to see if we should drop the packet, loop if so */
    } while (ctx->ictx.detune &&
             !qfDetunePacket(ctx->ictx.detune, &pbuf->ptime, pbuf->iplen));
#endif
    
    /* Handle fragmentation if necessary */
    if (fraginfo && fraginfo->frag) {
        yfDefragPBuf(ctx->fragtab, fraginfo, pbuf, pkt, caplen);
    }
}

/* Old pre-detuned code */
//static void qfTraceHandle(qfTraceSource_t             *lts,
//                          qfContext_t                 *ctx)
//{
//    yfPBuf_t                    *pbuf;
//    yfIPFragInfo_t              fraginfo_buf,
//                                *fraginfo = ctx->fragtab ?
//                                &fraginfo_buf : NULL;
//    libtrace_linktype_t         linktype;
//    uint8_t                     *pkt;
//    uint32_t                    caplen;
//    
//    struct timeval tv;
//    
//    /* get next spot in ring buffer */
//    pbuf = (yfPBuf_t *)rgaNextHead(ctx->pbufring);
//    g_assert(pbuf);
//    
//    /* extract data from libtrace */
//    tv = trace_get_timeval(lts->packet);
//    pkt = trace_get_packet_buffer(lts->packet, &linktype, &caplen);
//    
//    /* Decode packet into packet buffer */
//    if (!yfDecodeToPBuf(ctx->dectx,
//                        linktype,
//                        yfDecodeTimeval(&tv),
//                        trace_get_capture_length(lts->packet),
//                        pkt, fraginfo, pbuf))
//    {
//        /* Couldn't decode packet; counted in dectx. Skip. */
//        return;
//    }
//    
//    /* Handle fragmentation if necessary */
//    if (fraginfo && fraginfo->frag) {
//        yfDefragPBuf(ctx->fragtab, fraginfo, pbuf, pkt, caplen);
//    }
//}


static void qfTraceUpdateStats(qfTraceSource_t *lts) {
    yaf_trace_drop = (uint32_t) trace_get_dropped_packets(lts->trace);
    if (yaf_trace_drop == (uint32_t)-1) yaf_trace_drop = 0;
}

static gboolean qfTracePeriodicExport(
                                 qfContext_t         *ctx,
                                 uint64_t            ctime)
{
    /* check stats timer */
    if (ctx->octx.stats_period &&
        ctime - ctx->octx.stats_last >= ctx->octx.stats_period)
    {
        /* Stats timer, export */
        if (!yfWriteStatsRec(ctx, &ctx->err)) {
            return FALSE;
        }
        ctx->octx.stats_last = ctime;
    }
    
    /* check template timer */
    if (ctx->octx.template_rtx_period &&
        ctime - ctx->octx.template_rtx_last >= ctx->octx.template_rtx_period)
    {
        /* Template timer, export */
        if (!fbSessionExportTemplates(fBufGetSession(ctx->octx.fbuf), &ctx->err)) {
            return FALSE;
        }
        ctx->octx.template_rtx_last = ctime;
    }
    
    return TRUE;
}

gboolean qfTraceMain(qfContext_t             *ctx)
{
    gboolean                ok = TRUE;
    qfTraceSource_t         *lts = ctx->ictx.pktsrc;
    libtrace_err_t          terr;
    
    int i, trv;
    
    /* process input until we're done */
    while (!yaf_quit) {
        
        for (i = 0;
             i < TRACE_PACKET_GROUP && !yaf_quit &&
                (trv = trace_read_packet(lts->trace, lts->packet)) > 0;
             i++)
        {
            qfTraceHandle(lts, ctx);
        }
        
        /* Check for error */
        if (trace_is_err(lts->trace)) {
            terr = trace_get_err(lts->trace);
            g_warning("libtrace error: %s", terr.problem);
            ok = FALSE;
            break;
        }

        /* Check for quit or EOF */
        if (yaf_quit || trv == 0) break;

        /* Process the packet buffer */
        if (ok && !yfProcessPBufRing(ctx, &(ctx->err))) {
            ok = FALSE;
            break;
        }
        
        /* Do periodic export as necessary */
        qfTracePeriodicExport(ctx, yfFlowTabCurrentTime(ctx->flowtab));
    }

    return yfFinalFlush(ctx, ok,  &(ctx->err));
}
