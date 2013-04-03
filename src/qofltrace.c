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
#include <yaf/autoinc.h>
#include <yaf/yafcore.h>
#include <yaf/yaftab.h>
#include "yafout.h"
#include "yaflush.h"
#include "yafstat.h"

#include "qofltrace.h"

#include <libtrace.h>

struct qfTraceSource_st {
    libtrace_t          *trace;
    libtrace_packet_t   *packet;
    libtrace_filter_t   *filter;
};

static void qfTraceFree(qfTraceSource_t *lts) {
    if (lts->filter) trace_destroy_filter(lts->filter);
    if (lts->packet) trace_destroy_packet(lts->packet);
    if (lts->trace) trace_destroy(lts->trace);
    if (lts) g_free(lts);
}

qfTraceSource_t *qfTraceOpen(const char *uri,
                             const char *bpf,
                             GError **err)
{
    qfTraceSource_t *lts;
    libtrace_err_t  terr;
    
    lts = g_new0(qfTraceSource_t, 1);

    if (bpf) {
        if (!(lts->filter = trace_create_filter(bpf))) {
            g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                        "Count not compile libtrace BPF %s", bpf);
            goto err;
        }
    }
    
    if (!(lts->packet = trace_create_packet())) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "Could not initialize libtrace packet");
        goto err;
    }
    
    lts->trace = trace_create(uri);
    if (trace_is_err(lts->trace)) {
        terr = trace_get_err(lts->trace);
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "Count not open libtrace URI %s: %s",
                    uri, terr.problem);
        goto err;
    }

    if (trace_start(lts->trace) == -1) {
        terr = trace_get_err(lts->trace);
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "Count not start processing libtrace URI %s: %s",
                    uri, terr.problem);
        goto err;
    }
  
err:
    qfTraceFree(lts);
    return NULL;
}

void qfTraceClose(qfTraceSource_t *lts)
{
    
}

static void qfTraceHandle(qfTraceSource_t             *lts,
                          yfContext_t                 *ctx)
{
    yfPBuf_t                    *pbuf;
    yfIPFragInfo_t              fraginfo_buf,
                                *fraginfo = ctx->fragtab ?
                                &fraginfo_buf : NULL;
    uint8_t                     *pkt;
    size_t                      caplen;
    
    struct timeval tv;
    
    /* shortcut if filter doesn't match */
    if (lts->filter && trace_apply_filter(lts->filter, lts->packet) < 1) {
        return;
    }
    
    /* get next spot in ring buffer */
    pbuf = (yfPBuf_t *)rgaNextHead(ctx->pbufring);
    g_assert(pbuf);
    
    /* extract data from libtrace */
    tv = trace_get_timeval(lts->packet);
    pkt = trace_get_packet_buffer(lts->packet, &linktype, &caplen);
    yfDecodeSetLinktype(linktype);
    
    /* Decode packet into packet buffer */
    if (!yfDecodeToPBuf(ctx->dectx,
                        yfDecodeTimeval(&tv),
                        trace_get_capture_length(lts->packet),
                        pkt, fraginfo, pbuf))
    {
        /* Couldn't decode packet; counted in dectx. Skip. */
        return;
    }
    
    /* Handle fragmentation if necessary */
    if (fraginfo && fraginfo->frag) {
        if (!yfDefragPBuf(ctx->fragtab, fraginfo,
                          pbuf, pkt, hdr->caplen))
        {
            /* No complete defragmented packet available. Skip. */
            return;
        }
    }    
}

gboolean qfTraceMain(yfContext_t             *ctx)
{
    gboolean                ok = TRUE;
    qfTraceSource_t         *lts = (qfTraceSource_t *)ctx->pktsrc;
    
    GTimer                  *stimer = NULL;  /* to export stats */
    libtrace_err_t          terr;

    
    /* process input until we're done */
    while (!yaf_quit && (trace_read_packet(lts->trace, lts->packet) > 0)) {
        qfTraceHandle(lts);

        /* Process the packet buffer */
        if (ok && !yfProcessPBufRing(ctx, &(ctx->err))) {
            ok = FALSE;
            break;
        }

        /* send stats record on a timer */
        if (ok && !ctx->cfg->nostats) {
            if (g_timer_elapsed(stimer, NULL) > ctx->cfg->stats) {
                if (!yfWriteStatsRec(ctx, yaf_pcap_drop, yfStatGetTimer(),
                                     &(ctx->err)))
                {
                    ok = FALSE;
                    break;
                }
                g_timer_start(stimer);
                yaf_stats_out++;
            }
        }
    }

    /* Handle aftermath */
    if (trace_is_err(lts->trace)) {
        terr = trace_get_err(lts->trace);
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "libtrace error: %s", terr.problem);
        ok = FALSE;
    }
        
    /* Handle final flush */
    if (!ctx->cfg->nostats) {
        /* add one for final flush */
        if (ok) {
            yaf_stats_out++;
        }
        /* free timer */
        g_timer_destroy(stimer);
    }
    
    return yfFinalFlush(ctx, ok, 0, yfStatGetTimer(), &(ctx->err));
}