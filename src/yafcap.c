/**
 * @internal
 *
 ** yafcap.c
 ** YAF libpcap input support
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
#include <yaf/yafcore.h>
#include <yaf/yaftab.h>
#include "yafout.h"

#include <airframe/airlock.h>
#include <airframe/airutil.h>
#include <pcap.h>

#ifdef YAF_ENABLE_BIVIO
#include <pcap-zcopy.h>
#endif

#include "yafcap.h"
#include "yaflush.h"
#include "yafstat.h"

struct yfCapSource_st {
    pcap_t          *pcap;
    FILE            *lfp;
    gboolean        is_live;
    int             datalink;
};

static pcap_t *yaf_pcap;

static GTimer *timer_pcap_file = NULL;

/* Quit flag support */
extern int yaf_quit;

extern yfConfig_t yaf_config;

/* Statistics */
static uint32_t            yaf_pcap_drop = 0;
static uint32_t            yaf_stats_out = 0;

/* One second timeout for capture loop */
#define YAF_CAP_TIMEOUT 1000

/* Process at most 64 packets at once */
#define YAF_CAP_COUNT   64

static gboolean yfCapCheckDatalink(
    pcap_t                  *pcap,
    int                     *datalink,
    GError                  **err)
{
    /* verify datalink */
    *datalink = pcap_datalink(pcap);

    switch (*datalink) {
#ifdef DLT_EN10MB
      case DLT_EN10MB:
#endif
#ifdef DLT_C_HDLC
      case DLT_C_HDLC:
#endif
#ifdef DLT_LINUX_SLL
      case DLT_LINUX_SLL:
#endif
#ifdef DLT_PPP
      case DLT_PPP:
#endif
#ifdef DLT_PPP_ETHER
      case DLT_PPP_ETHER:
#endif
#ifdef DLT_RAW
      case DLT_RAW:
#endif
#ifdef DLT_NULL
      case DLT_NULL:
#endif
#ifdef DLT_LOOP
      case DLT_LOOP:
#endif
        break;
      case -1:
          g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
              "Unable to access pcap datalink, (superuser access?)");
          return FALSE;
      default:
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "Unsupported pcap datalink type %u", *datalink);
        return FALSE;
    }

    return TRUE;
}

static pcap_t *yfCapOpenFileInner(
    const char              *path,
    int                     *datalink,
    GError                  **err)
{
    pcap_t                  *pcap;
    static char             pcap_errbuf[PCAP_ERRBUF_SIZE];

    if ((strlen(path) == 1) && path[0] == '-') {
        /* Don't open stdin if it's a terminal */
        if (isatty(fileno(stdin))) {
            g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                "Refusing to read from terminal on stdin");
            return NULL;
        }
    }

    pcap = pcap_open_offline(path, pcap_errbuf);
    if (!pcap) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "%s", pcap_errbuf);
        return NULL;
    }

    if (!yfCapCheckDatalink(pcap, datalink, err)) {
        pcap_close(pcap);
        return NULL;
    }

    g_debug("Reading packets from %s", path);

    return pcap;
}

yfCapSource_t *yfCapOpenFile(
    const char              *path,
    int                     *datalink,
    GError                  **err)
{
    yfCapSource_t           *cs;

    cs = g_new0(yfCapSource_t, 1);
    cs->pcap = yfCapOpenFileInner(path, datalink, err);
    cs->is_live = FALSE;
    cs->lfp = NULL;
    cs->datalink = *datalink;

    if (!cs->pcap) {
        g_free(cs);
        cs = NULL;
    }

    return cs;
}

static gboolean yfCapFileListNext(
    yfCapSource_t           *cs,
    GError                  **err)
{
    static char             cappath[FILENAME_MAX+1];
    size_t                  cappath_len;
    int                     this_datalink;

    /* close the present pcap if necessary */
    if (cs->pcap) {
        pcap_close(cs->pcap);
        cs->pcap = NULL;
    }

    /* keep going until we get an actual opened pcap file */
    while (1) {

        /* get the next line from the name list file */
        if (!fgets(cappath, FILENAME_MAX, cs->lfp)) {
            if (feof(cs->lfp)) {
                g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_EOF,
                    "End of pcap file list");
            } else {
                g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "Couldn't read pcap file list: %s", strerror(errno));
            }
            return FALSE;
        }

        /* ensure filename is null terminated */
        cappath[FILENAME_MAX] = (char)0;

        /* skip comments and blank lines */
        if (cappath[0] == '\n' || cappath[0] == '#') {
            continue;
        }

        /* remove trailing newline */
        cappath_len = strlen(cappath);
        if (cappath[cappath_len-1] == '\n') {
            cappath[cappath_len-1] = (char)0;
        }

        /* we have what we think is a filename. try opening it. */
        cs->pcap = yfCapOpenFileInner(cappath, &this_datalink, err);
        if (!cs->pcap) {
            g_warning("skipping pcap file %s due to error: %s.",
                cappath, (*err)->message);
            g_clear_error(err);
            continue;
        }

        /* make sure the datalink matches all the others */
        if (cs->datalink == -1) {
            cs->datalink = this_datalink;
        } else if (cs->datalink != this_datalink) {
            g_warning("skipping pcap file %s due to mismatched "
                "datalink type %u (expecting %u).",
                cappath, this_datalink, cs->datalink);
            pcap_close(cs->pcap);
            cs->pcap = NULL;
            continue;
        }

        /* We have a file. All is well. */
        return TRUE;
    }
}

yfCapSource_t *yfCapOpenFileList(
    const char              *path,
    int                     *datalink,
    GError                  **err)
{
    yfCapSource_t           *cs;

    /* allocate a new capsource */
    cs = g_new0(yfCapSource_t, 1);

    /* handle file list from stdin */
    if ((strlen(path) == 1) && path[0] == '-') {
        /* Don't open stdin if it's a terminal */
        if (isatty(fileno(stdin))) {
            g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                "Refusing to read from terminal on stdin");
            g_free(cs);
            return NULL;
        }
        cs->lfp = stdin;
    } else {
        /* open file list file */
        cs->lfp = fopen(path, "r");
        if (!cs->lfp) {
            g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                "Couldn't open pcap file list: %s", strerror(errno));
            g_free(cs);
            return NULL;
        }
    }

    /* note we're not live */
    cs->is_live = FALSE;

    /* note we have no datalink yet */
    cs->datalink = -1;

    /* open the first pcap file in the file list */
    if (!yfCapFileListNext(cs, err)) {
        fclose(cs->lfp);
        g_free(cs);
        return NULL;
    }

    /* copy datalink back out of capsource */
    *datalink = cs->datalink;

    /* all done */
    return cs;
}

static gboolean yfSetPcapFilter(
    pcap_t             *pcap,
    const char         *bpf_expr,
    GError             **err)
{
    struct bpf_program bpf;

    /* attach filter */
    if (pcap_compile(pcap, &bpf, bpf_expr, 1, 0) < 0) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                    "couldn't compile BPF expression %s: %s",
                    bpf_expr, pcap_geterr(pcap));
        return FALSE;
    }
    if ( pcap_setfilter(pcap, &bpf) ) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                    "couldn't compile BPF expression %s: %s",
                    bpf_expr, pcap_geterr(pcap));
        return FALSE;
    }
    pcap_freecode(&bpf);
    return TRUE;
}


yfCapSource_t *yfCapOpenLive(
    const char              *ifname,
    int                     snaplen,
    int                     *datalink,
    GError                  **err)
{
    yfCapSource_t           *cs;
    pcap_t                  *pcap;
    static char             pcap_errbuf[PCAP_ERRBUF_SIZE];

    pcap = pcap_open_live(ifname, snaplen, 1,
                          YAF_CAP_TIMEOUT, pcap_errbuf);
    if (!pcap) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "%s", pcap_errbuf);
        return NULL;
    }

    if (!yfCapCheckDatalink(pcap, datalink, err)) {
        pcap_close(pcap);
        return NULL;
    }

#ifdef YAF_ENABLE_BIVIO
    if (!pcap_is_zcopy(pcap)) {
        g_warning("ZCOPY Not enabled on Bivio");
    }
#endif

    cs = g_new0(yfCapSource_t, 1);
    cs->pcap = pcap;
    cs->is_live = TRUE;
    cs->lfp = NULL;
    cs->datalink = *datalink;

    return cs;
}

void yfCapClose(
    yfCapSource_t                 *cs)
{
    if (cs->pcap) {
        pcap_close(cs->pcap);
    }
    if (cs->lfp) {
        fclose(cs->lfp);
    }
    g_free(cs);
}

static void yfCapUpdateStats(pcap_t *pcap) {
    struct pcap_stat ps;

    if (pcap_stats(pcap, &ps) != 0) {
        g_warning("couldn't get statistics: %s", pcap_geterr(pcap));
        return;
    }

    yaf_pcap_drop = ps.ps_drop;
}

void yfCapDumpStats() {
    if (yaf_stats_out) {
        g_debug("yaf Exported %u stats records.", yaf_stats_out);
    }

    if (yaf_pcap_drop) {
        g_warning("Live capture device dropped %u packets.", yaf_pcap_drop);
    }
}

static pcap_dumper_t *yfCapPcapRotate(
    yfContext_t        *ctx)
{
    pcap_dumper_t      *pcap_ret = NULL;
    GString            *namebuf = g_string_new("");
    AirLock            *lock = &(ctx->pcap_lock);
    GError             *err = NULL;
    static uint32_t    serial = 0;

    if (ctx->pcap) {
        pcap_dump_flush(ctx->pcap);
        pcap_dump_close(ctx->pcap);
        air_lock_release(lock);
    }

    ctx->pcap_offset = 24;

    g_string_append_printf(namebuf, "%s/yaf_", ctx->cfg->pcapdir);
    air_time_g_string_append(namebuf, time(NULL), AIR_TIME_SQUISHED);
    g_string_append_printf(namebuf, "_%05u.pcap", serial++);

    air_lock_acquire(lock, namebuf->str, &err);

    yfUpdateRollingPcapFile(ctx->flowtab, namebuf);

    pcap_ret = pcap_dump_open(yaf_pcap, namebuf->str);

    if (pcap_ret == NULL) {
        g_warning("Could not open new rolling pcap file: %s",
                  pcap_geterr(yaf_pcap));
        g_warning("Turning off pcap export...");
        ctx->cfg->pcapdir = NULL;
    }

    g_string_free(namebuf, TRUE);

    if (ctx->cfg->pcap_timer) {
        if ( !timer_pcap_file ) {
            timer_pcap_file = g_timer_new();
        }
        g_timer_start(timer_pcap_file);
    }

    return pcap_ret;
}


/**
 * yfCapHandle
 *
 * This is the function that gets the call back from the PCAP library
 * when a packet arrives; it does not get called directly from within
 * yaf.
 *
 * @param ctx opaque pointer to PCAP, holds the YAF context for the capture
 * @param hdr PCAP capture details (time, packet length, capture length)
 * @param pkt pointer to the captured packet
 *
 */
static void yfCapHandle(
    yfContext_t                 *ctx,
    const struct pcap_pkthdr    *hdr,
    const uint8_t               *pkt)
{
    yfPBuf_t                    *pbuf;
#ifdef YAF_ENABLE_BIVIO
    yfCapSource_t               *cs = (yfCapSource_t *)ctx->pktsrc;
    int                         iface = 0;
#endif
    yfIPFragInfo_t              fraginfo_buf,
                                *fraginfo = ctx->fragtab ?
                                            &fraginfo_buf : NULL;
    /* get next spot in ring buffer */
    pbuf = (yfPBuf_t *)rgaNextHead(ctx->pbufring);
    g_assert(pbuf);

    /* pcap-per-flow info to pass to decode */
    pbuf->pcap_hdr.ts = hdr->ts;
    pbuf->pcap_hdr.len = hdr->len;
    pbuf->pcap_hdr.caplen = hdr->caplen;
    pbuf->pcapt = yaf_pcap;

#ifdef YAF_ENABLE_BIVIO
    iface = pcap_zcopy_get_origin(cs->pcap, pkt);
    if (iface < 0) {
        g_warning("Unable to retrieve interface ID %s", pcap_geterr(cs->pcap));
    } else {
        pbuf->key.netIf = iface;
    }
#endif

    /* rolling pcap dump */
    if (ctx->pcap) {
        pcap_dump((u_char *)ctx->pcap, hdr, pkt);
    }

    /* Decode packet into packet buffer */
    if (!yfDecodeToPBuf(ctx->dectx,
                        yfDecodeTimeval(&(hdr->ts)),
                        hdr->caplen, pkt,
                        fraginfo, ctx->pbuflen, pbuf))
    {
        /* Couldn't decode packet; counted in dectx. Skip. */
        return;
    }

    if (ctx->pcap) {
        pbuf->pcap_offset = ctx->pcap_offset;
        ctx->pcap_offset += (16 + pbuf->pcap_hdr.caplen);
    }

    /* Handle fragmentation if necessary */
    if (fraginfo && fraginfo->frag) {
        if (!yfDefragPBuf(ctx->fragtab, fraginfo,
                          ctx->pbuflen, pbuf, pkt, hdr->caplen))
        {
            /* No complete defragmented packet available. Skip. */
            return;
        }
    }

}

/**
 * yfCapMain
 *
 *
 *
 *
 */
gboolean yfCapMain(
    yfContext_t             *ctx)
{
    AirLock                 lockbuf = AIR_LOCK_INIT, *lock = NULL;
    gboolean                ok = TRUE;
    gboolean                buf_excess = FALSE;
    yfCapSource_t           *cs = (yfCapSource_t *)ctx->pktsrc;
    int                     pcrv = 0;
    char                    *bp_filter= (char *)ctx->cfg->bpf_expr;
    GTimer                  *stimer = NULL;  /* to export stats */

    /* set up output locking in lock mode */
    if (ctx->cfg->lockmode) {
        lock = &lockbuf;
    }

    if (!ctx->cfg->nostats) {
        stimer = g_timer_new();
    }

    if (ctx->cfg->pcapdir) {
        if (!yfTimeOutFlush(ctx, yaf_pcap_drop, &yaf_stats_out,
                            yfStatGetTimer(), stimer,
                            &(ctx->err)))
        {
            yaf_quit = TRUE;
        }
    }

#ifdef YAF_ENABLE_BIVIO
    if (pcap_zcopy_add_all_interfaces(cs->pcap) == -1) {
        g_warning("Error adding zcopy interfaces %s", pcap_geterr(cs->pcap));
    }
#endif

    if (ctx->cfg->pcapdir && !ctx->cfg->pcap_per_flow) {
        yaf_pcap = cs->pcap;
        ctx->pcap = yfCapPcapRotate(ctx);
    }

    /* process input until we're done */
    while (!yaf_quit) {

        yaf_pcap = cs->pcap;

        if (bp_filter) {
            yfSetPcapFilter(cs->pcap, bp_filter, &(ctx->err));
        }
        /* Process some packets */
        pcrv = pcap_dispatch(cs->pcap, YAF_CAP_COUNT,
                             (pcap_handler)yfCapHandle, (void *)ctx);

        /* Handle the aftermath */
        if (pcrv == 0) {
            /* No packet available */
            if (cs->lfp) {
                /* Advance to next capfile */
                if (!yfCapFileListNext(cs, &(ctx->err))) {
                    if (!g_error_matches(ctx->err, YAF_ERROR_DOMAIN,
                                         YAF_ERROR_EOF))
                    {
                        ok = FALSE;
                    }
                    buf_excess = TRUE;
                    g_clear_error(&(ctx->err));
                    break;
                }
                yfDecodeResetOffset(ctx->dectx);

            } else if (!cs->is_live) {
             /* EOF in single capfile mode; break; will check to see if
                excess in buffer */
                buf_excess = TRUE;
                break;
            } else {
                /* Live, no packet processed (timeout). Flush buffer */
                if (!yfTimeOutFlush(ctx, yaf_pcap_drop, &yaf_stats_out,
                                    yfStatGetTimer(), stimer,
                                    &(ctx->err)))
                {
                    ok = FALSE;
                    break;
                }
                continue;
            }
        } else if (pcrv < 0) {
            if (ctx->cfg->noerror && cs->lfp) {
                if (yfCapFileListNext(cs, &(ctx->err))) {
                    g_warning("Couldn't read next pcap record from %s: %s",
                        ctx->cfg->inspec, pcap_geterr(cs->pcap));
                } else {
                    /* An error occurred reading packets. */
                    g_set_error(&(ctx->err), YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                                "Couldn't read next pcap record from %s: %s",
                                ctx->cfg->inspec, pcap_geterr(cs->pcap));
                    ok = FALSE;
                    break;
                }
                yfDecodeResetOffset(ctx->dectx);
            } else {
                if (ctx->cfg->noerror) {
                    g_warning("Couldn't read next pcap record from %s: %s",
                              ctx->cfg->inspec, pcap_geterr(cs->pcap));
                    ok = TRUE;
                } else {
                    /* An error occurred reading packets. */
                    g_set_error(&(ctx->err), YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                                "Couldn't read next pcap record from %s: %s",
                                ctx->cfg->inspec, pcap_geterr(cs->pcap));
                    ok = FALSE;
                }
                break;
            }
        }

        /* Process the packet buffer */
        if (ok && !yfProcessPBufRing(ctx, &(ctx->err))) {
            ok = FALSE;
            break;
        }

        if (ok && !ctx->cfg->nostats) {
            if (g_timer_elapsed(stimer, NULL) > ctx->cfg->stats) {
                if (!yfWriteStatsFlow(ctx, yaf_pcap_drop, yfStatGetTimer(),
                                      &(ctx->err)))
                {
                    ok = FALSE;
                    break;
                }
                g_timer_start(stimer);
                yaf_stats_out++;
            }
        }

        if (ok && ctx->cfg->pcapdir && !ctx->cfg->pcap_per_flow) {
            if (!ctx->pcap || (ftell(pcap_dump_file(ctx->pcap)) >
                               ctx->cfg->max_pcap) || (timer_pcap_file &&
                               (g_timer_elapsed(timer_pcap_file, NULL) >
                                ctx->cfg->pcap_timer)))
            {
                ctx->pcap = yfCapPcapRotate(ctx);
            }
        }
    }

    /* Process any excess in packet buffer */
    if (buf_excess) {
        if (ok && !yfProcessPBufRing(ctx, &(ctx->err))) {
            ok = FALSE;
        }
    }

    /* Update packet drop statistics for live capture */
    if (cs->is_live) {
        yfCapUpdateStats(cs->pcap);
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

    if (ctx->pcap) {
        pcap_dump_flush(ctx->pcap);
        pcap_dump_close(ctx->pcap);
        air_lock_release(&(ctx->pcap_lock));
        air_lock_cleanup(&(ctx->pcap_lock));
        if (timer_pcap_file) {
            g_timer_destroy(timer_pcap_file);
        }
    }

    return yfFinalFlush(ctx, ok, yaf_pcap_drop, yfStatGetTimer(),
                        &(ctx->err));
}
