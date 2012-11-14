/*
 ** yafpcapx.c
 ** YAF Napatech support using pcapexpress library (libpcapexpress)
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2006-2012 Carnegie Mellon University. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammel, Napatech, Emily Sarneso <ecoff@cert.org>
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
#include <yaf/autoinc.h>

#if YAF_ENABLE_NAPATECH
#include "yafout.h"
#include "yafpcapx.h"
#include "yaftab.h"
#include <yaf/yafcore.h>
#include <yaf/yaftab.h>
#include <airframe/privconfig.h>
#include <airframe/airlock.h>
#include "yafstat.h"
#include "yaflush.h"

/* Statistics */
static uint64_t     yaf_pcapx_captured = 0;
static uint64_t     yaf_pcapx_dropped = 0;
static uint64_t     yaf_pcapx_bytes = 0;
static uint32_t     yaf_stats_out = 0;
GTimer *stimer = NULL;

/* Quit flag support */
extern int yaf_quit;

yfPcapxSource_t *yfPcapxOpenLive(
    const char              *ifname,
    int                     snaplen,
    int                     *datalink,
    GError                  **err)
{
    yfPcapxSource_t          *ps = NULL;
    uint32_t                 adapter;
    uint32_t                 feed;

    if ( ifname[1] != ':' ) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "Invalid interface %s", ifname);
        goto err;
    }

    adapter = ifname[0] - '0';

    feed = atoi(&ifname[2]);

    if ( !(ps = pcapexpress_open(adapter, feed)) ) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "Couldn't open %s", ifname);
        goto err;
    }

    *datalink = DLT_EN10MB;

    /* return context */
    return ps;

err:
    /* tear down the Napatech/PcapExpress context */
    yfPcapxClose(ps);
    return NULL;
}

void yfPcapxClose(
    yfPcapxSource_t          *ps)
{
    pcapexpress_close(ps);
    ps = NULL;
}

gboolean yfPcapxMain(
    yfContext_t             *ctx)
{
    gboolean                ok = TRUE;
    yfPcapxSource_t         ps = (yfPcapxSource_t)ctx->pktsrc;
    yfPBuf_t                *pbuf = NULL;
    yfIPFragInfo_t          fraginfo_buf,
                            *fraginfo = ctx->fragtab ?
                                        &fraginfo_buf : NULL;
    int32_t                 status = 0;
    uint8_t                 *frame = NULL;
    PCAPX_HEADER            *descriptor = NULL;

    /* create stats timer if starts are turned on */
    if (!ctx->cfg->nostats) {
        stimer = g_timer_new();
    }

    /* process input until we're done */
    while (!yaf_quit) {

        status = pcapexpress_next_frame(ps, &descriptor, &frame);
        if (status < 0) {
            g_warning("Error reading Napatech feed\n");
            ok = FALSE;
            break;
        } else if (status == 0) {
            /* Live, no packet processed (timeout). Flush buffer */
            if (!yfTimeOutFlush(ctx, (uint32_t)yaf_pcapx_dropped,
                                &yaf_stats_out, yfStatGetTimer(),
                                stimer, &(ctx->err)))
            {
                ok = FALSE;
                break;
            }
            continue;
        }

        /* Grab a packet buffer from ring head */
        if (!(pbuf = (yfPBuf_t *)rgaNextHead(ctx->pbufring))) {
            break;
        }

#if YAF_ENABLE_NAPATECH_SEPARATE_INTERFACES
        /* if enabled, record the Napatech interface */
        if (((PCAPX_HEADER2 *)descriptor)->headerType) {
            pbuf->key.netIf = ((PCAPX_HEADER2 *)descriptor)->channel;
        }
#endif

        /* Decode packet into packet buffer */
        if (!yfDecodeToPBuf(ctx->dectx,
                            yfDecodeTimeval((struct timeval *)&descriptor->ts),
                            descriptor->storelen, frame,
                            fraginfo, pbuf))
        {
            /* No packet available. Skip. */
            continue;
        }

        /* Handle fragmentation if necessary */
        if (fraginfo && fraginfo->frag) {
            if (!yfDefragPBuf(ctx->fragtab, fraginfo,
                              pbuf, frame, descriptor->storelen))
            {
                /* No complete defragmented packet available. Skip. */
                continue;
            }
        }

        /* Process the packet buffer */
        if (ok && !yfProcessPBufRing(ctx, &(ctx->err))) {
            ok = FALSE;
            break;
        }

        if (!ctx->cfg->nostats) {
            if (g_timer_elapsed(stimer, NULL) > ctx->cfg->stats) {
                if (!yfWriteStatsFlow(ctx, (uint32_t)yaf_pcapx_dropped,
                                      yfStatGetTimer(), &(ctx->err)))
                {
                    ok = FALSE;
                    break;
                }
                g_timer_start(stimer);
                yaf_stats_out++;
            }
        }
    }

    /* Update packet drop statistics for live capture */
    pcapexpress_statistics(ps, &yaf_pcapx_captured, &yaf_pcapx_dropped,
                           &yaf_pcapx_bytes);

    if (!ctx->cfg->nostats) {
        /* add one for final flush */
        if (ok) yaf_stats_out++;
        g_timer_destroy(stimer);
    }

    /* Handle final flush */
    return yfFinalFlush(ctx, ok, (uint32_t)yaf_pcapx_dropped,
                        yfStatGetTimer(), &(ctx->err));
}



void yfPcapxDumpStats() {

    g_warning("yaf Exported %u stats records.", yaf_stats_out);
    g_warning("Live capture device: captured %lu, dropped %lu, bytes %lu",
              yaf_pcapx_captured,
              yaf_pcapx_dropped,
              yaf_pcapx_bytes);
}

#endif
