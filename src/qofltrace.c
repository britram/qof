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

#if YAF_ENABLE_LIBTRACE

#include <yaf/yafcore.h>
#include <yaf/yaftab.h>
#include <yaf/decode.h>
#include "yafout.h"
#include "yaflush.h"
#include "yafstat.h"

#include "qofltrace.h"

#define TRACE_PACKET_GROUP 32

/* Quit flag support */
extern int yaf_quit;

extern yfConfig_t yaf_config;

/* Statistics */
static uint32_t            yaf_trace_drop = 0;
static uint32_t            yaf_stats_out = 0;

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
            return FALSE;
        }
        
        if (trace_config(lts->trace, TRACE_OPTION_FILTER, lts->filter) == -1) {
            terr = trace_get_err(lts->trace);
            g_warning("Could not set libtrace filter: %s", terr.problem);
            return FALSE;
        }
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

static void qfTraceHandle(qfTraceSource_t             *lts,
                          qfContext_t                 *ctx)
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
    
    /* extract data from libtrace */
    tv = trace_get_timeval(lts->packet);
    pkt = trace_get_packet_buffer(lts->packet, &linktype, &caplen);
    yfDecodeSetLinktype(ctx->dectx, linktype);
    
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
        yfDefragPBuf(ctx->fragtab, fraginfo, pbuf, pkt, caplen);
    }
}

static void qfTraceUpdateStats(qfTraceSource_t *lts) {
    yaf_trace_drop = (uint32_t) trace_get_dropped_packets(lts->trace);
    if (yaf_trace_drop == (uint32_t)-1) yaf_trace_drop = 0;
}

gboolean qfTraceMain(qfContext_t             *ctx)
{
    gboolean                ok = TRUE;
    qfTraceSource_t         *lts = (qfTraceSource_t *)ctx->pktsrc;
    char                    *bpf = (char *)ctx->cfg->bpf_expr;
    GTimer                  *stimer = NULL;  /* to export stats */
    libtrace_err_t          terr;
    
    int i, trv;
    
    /* start stats timer */
    if (!ctx->cfg->nostats) {
        stimer = g_timer_new();
    }    

    /* start processing */
    if (trace_start(lts->trace) == -1) {
        terr = trace_get_err(lts->trace);
        g_warning("libtrace trace_start() error: %s", terr.problem);
        return FALSE;
    }

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

        /* send stats record on a timer */
        if (ok && !ctx->cfg->nostats) {
            if (g_timer_elapsed(stimer, NULL) > ctx->cfg->stats) {
                qfTraceUpdateStats(lts);
                if (!yfWriteStatsRec(ctx,
                                     yaf_trace_drop,
                                     yfStatGetTimer(),
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

    /* Handle final flush */
    if (!ctx->cfg->nostats) {
        /* add one for final flush */
        if (ok) {
            qfTraceUpdateStats(lts);
            yaf_stats_out++;
        }
        /* free timer */
        g_timer_destroy(stimer);
    }
    
    return yfFinalFlush(ctx, ok, yaf_trace_drop,
                        yfStatGetTimer(), &(ctx->err));
}

void qfTraceDumpStats() {
    if (yaf_stats_out) {
        g_debug("yaf exported %u stats records.", yaf_stats_out);
    }
    
    if (yaf_trace_drop) {
        g_warning("libtrace dropped %u packets.", yaf_trace_drop);
    }
}

#endif