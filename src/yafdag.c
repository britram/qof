/**
 ** yafdag.c
 ** YAF Endace DAG live input support
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
#include <yaf/autoinc.h>

#if YAF_ENABLE_DAG
#include "yafout.h"
#include "yafdag.h"
#include "yaftab.h"
#include "yafstat.h"
#include <yaf/yafcore.h>
#include <yaf/yaftab.h>
#include <airframe/privconfig.h>
#include <airframe/airlock.h>
#include <dagapi.h>
#include <pcap.h>
#include "yaflush.h"

/* Statistics */
static uint32_t     yaf_dag_drop = 0;
static uint32_t     yaf_stats_out = 0;
GTimer *stimer = NULL;  /* to export stats */

/* Quit flag support */
extern int yaf_quit;

struct yfDagSource_st {
    char                    name[DAGNAME_BUFSIZE];
    int                     stream;
    int                     fd;
    int                     datalink;
    gboolean                fd_opened;
    gboolean                stream_attached;
    gboolean                stream_started;
};

static void yaf_dag_timestamp(
    uint64_t                dts,
    struct timeval          *ts)
{
    double                  ddts;

    ddts = (dts & 0xFFFFFFFF00000000LL) >> 32;
    ddts += ((dts & 0x00000000FFFFFFFFLL) * 1.0) / (2LL << 32);

    ts->tv_sec = (uint32_t)ddts;
    ddts -= ts->tv_sec;
    ts->tv_usec = (uint32_t)(ddts * 1000000);
}

yfDagSource_t *yfDagOpenLive(
    const char              *ifname,
        int                                             snaplen,
    int                     *datalink,
    GError                  **err)
{
    yfDagSource_t          *ds = NULL;
    struct timeval          timeout, poll;

    /* Allocate a new DAG context */
    ds = g_new0(yfDagSource_t, 1);

    /* parse the device name to get the stream */
    if (dag_parse_name(ifname, ds->name,
                       DAGNAME_BUFSIZE, &ds->stream) < 0) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "Couldn't parse device name %s: %s",
                    ifname, strerror(errno));
        goto err;
    }

    /* open the DAG fd */
    if ((ds->fd = dag_open(ds->name)) < 0) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "Couldn't open %s: %s",
                    ds->name, strerror(errno));
        goto err;
    }
    ds->fd_opened = TRUE;

    /* configure the fd options */
    /* FIXME do we care about these? what are they? */
    if (dag_configure(ds->fd, "") < 0) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "Couldn't configure %s: %s",
                    ds->name, strerror(errno));
        goto err;
    }

    /* attach the stream */
    if (dag_attach_stream(ds->fd, ds->stream, 0, 0) < 0) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "Couldn't attach stream %u on %s: %s",
                    ds->stream, ds->name, strerror(errno));
        goto err;
    }
    ds->stream_attached = TRUE;

    /* start the stream */
    if (dag_start_stream(ds->fd, ds->stream) < 0) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "Couldn't start stream %u on %s: %s",
                    ds->stream, ds->name, strerror(errno));
        goto err;
    }
    ds->stream_started = TRUE;

    /* set polling parameters, 100ms timeout with 10ms polling interval. */
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    poll.tv_sec = 0;
    poll.tv_usec = 10000;
    dag_set_stream_poll(ds->fd, ds->stream, 32 * 1024, &timeout, &poll);

    /* set DAG linktype */
    switch (dag_linktype(ds->fd)) {
      case TYPE_ETH:
        ds->datalink = DLT_EN10MB;
        g_debug("Detected Ethernet DAG device %s (TYPE_ETH, DLT_EN10MB)",
            ds->name);
        break;
      case TYPE_MC_HDLC:
        ds->datalink = DLT_RAW;
        g_debug("Detected HDLC DAG device %s (TYPE_MC_HDLC, DLT_RAW)",
            ds->name);
        break;
      case TYPE_HDLC_POS:
        ds->datalink = DLT_RAW;
        g_debug("Detected HDLC DAG device %s (TYPE_HDLC_POS, DLT_RAW)",
            ds->name);
        break;
      default:
        ds->datalink = DLT_RAW;
        g_warning("Detected unsupported DAG device %s linktype %d; "
                  "no packets will be processed.",
                  ds->name, dag_linktype(ds->fd));
    }
    *datalink = ds->datalink;

    /* return dag context */
    return ds;

  err:
    /* tear down the dag context */
    yfDagClose(ds);
    return NULL;
}

void yfDagClose(
    yfDagSource_t          *ds)
{

    if (ds->fd_opened) {
        if (ds->stream_attached) {
            if (ds->stream_started) {
                dag_stop_stream(ds->fd, ds->stream);
            }
            dag_detach_stream(ds->fd, ds->stream);
        }
        dag_close(ds->fd);
    }

    g_free(ds);
}

gboolean yfDagMain(
    yfContext_t             *ctx)
{
    gboolean                ok = TRUE;
    yfDagSource_t           *ds = (yfDagSource_t *)ctx->pktsrc;
    yfPBuf_t                *pbuf = NULL;
    yfIPFragInfo_t          fraginfo_buf,
                            *fraginfo = ctx->fragtab ?
                                        &fraginfo_buf : NULL;
    uint8_t                 *cp = NULL, *ep = NULL, *fpp = NULL;
    dag_record_t            *rec;
    size_t                  caplen, reclen;

    if (!ctx->cfg->nostats) {
        stimer = g_timer_new();
    }

    /* process input until we're done */
    while (!yaf_quit) {

        /* advance the stream if necessary */
        if ((cp >= ep) &&
            !(ep = dag_advance_stream(ds->fd, ds->stream, &cp)))
        {
            g_warning("Couldn't advance stream %u on %s: %s",
                       ds->stream, ds->name, strerror(errno));
            ok = FALSE;
            break;
        }

        /* Process packets, defragmenting them */
        while (cp < ep) {

            /* Grab a packet buffer from ring head */
            if (!(pbuf = (yfPBuf_t *)rgaNextHead(ctx->pbufring))) {
                break;
            }

            /* mark it skippable to start */
            pbuf->ptime = 0;

            /* get the DAG record */
            rec = (dag_record_t *)cp;

            /* account for lost packets since last record */
            if (rec->lctr) {
                yaf_dag_drop += rec->lctr;
            }

            /* get length of captured data */
            reclen = g_ntohs(rec->rlen);

            /* advance pointer */
            cp += reclen;

            /* only process dag records matching the declared datalink */
            if (rec->type == TYPE_ETH &&
                ds->datalink == DLT_EN10MB)
            {
                /* skip pad to start of ethernet header */
                fpp = &(rec->rec.eth.dst[0]);
            } else if (rec->type == TYPE_MC_HDLC &&
                       ds->datalink == DLT_RAW)
            {
                /* skip to payload and treat as raw */
                fpp = &(rec->rec.mc_hdlc.pload[0]);
            } else if (rec->type == TYPE_HDLC_POS &&
                       ds->datalink == DLT_RAW)
            {
                fpp = &(rec->rec.pos.pload[0]);
            } else {
                continue;
            }

            /* remove dag and unused layer 2 headers from caplen */
            caplen = (((uint8_t *)rec + reclen) - fpp);

            #if YAF_ENABLE_DAG_SEPARATE_INTERFACES
            /* if enabled, record the DAG interface */
            pbuf->key.netIf = rec->flags.iface;
            #endif

            /* Decode packet into packet buffer */
            if (!yfDecodeToPBuf(ctx->dectx,
                                yfDecodeTimeNTP(rec->ts),
                                caplen, fpp,
                                fraginfo, pbuf))
            {
                /* No packet available. Skip. */
                continue;
            }

            /* Handle fragmentation if necessary */
            if (fraginfo && fraginfo->frag) {
                if (!yfDefragPBuf(ctx->fragtab, fraginfo,
                                  pbuf, fpp, caplen)) {
                    /* No complete defragmented packet available. Skip. */
                    continue;
                }
            }
        }

        /* Process the packet buffer */
        if (ok && !yfProcessPBufRing(ctx, &(ctx->err))) {
            ok = FALSE;
            break;
        }

        if (!ctx->cfg->nostats) {
            if (g_timer_elapsed(stimer, NULL) > ctx->cfg->stats) {
                if (!yfWriteStatsFlow(ctx, yaf_dag_drop, yfStatGetTimer(),
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

    if (!ctx->cfg->nostats) {
        /* add one for final flush */
        if (ok) yaf_stats_out++;
        /* free timer */
        g_timer_destroy(stimer);
    }

    /* Handle final flush */
    return yfFinalFlush(ctx, ok, yaf_dag_drop, yfStatGetTimer(),
                        &(ctx->err));
}

void yfDagDumpStats() {

    if (yaf_stats_out) {
        g_warning("yaf Exported %u stats records.", yaf_stats_out);
    }

    if (yaf_dag_drop) {
        g_warning("Live capture device dropped %u packets.", yaf_dag_drop);
    }
}

#endif
