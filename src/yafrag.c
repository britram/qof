/*
 ** yafrag.c
 ** YAF Active Fragment Table
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
#include <yaf/yafcore.h>
#include <yaf/decode.h>
#include <yaf/picq.h>
#include <yaf/yafrag.h>

/* max ip is 60, max tcp is 60, 14 for l2 */
#define YF_FRAG_L4H_MAX 134
#define YF_FRAG_TIMEOUT 30000
#define YF_FRAGPRUNE_DELAY 5000

typedef struct yfFragRec_st {
    uint16_t                off;
    uint16_t                len;
} yfFragRec_t;

typedef struct yfFragKey_st {
    uint32_t                ipid;
    yfFlowKey_t             f;
} yfFragKey_t;

typedef struct yfFragNode_st {
    struct yfFragNode_st        *p;
    struct yfFragNode_st        *n;
    struct yfFragTab_st         *tab;
    uint64_t                    last_ctime;
    gboolean                    have_first;
    gboolean                    have_last;
    gboolean                    have_l4hdr;
    yfFragKey_t                 key;
    yfTCPInfo_t                 tcpinfo;
    yfL2Info_t                  l2info;
    uint16_t                    iplen;
    GArray                      *records;
    size_t                      paylen;
    size_t                      payoff;
    uint8_t                     *payload;
} yfFragNode_t;

typedef struct yfFragQueue_st {
    yfFragNode_t      *tail;
    yfFragNode_t      *head;
} yfFragQueue_t;


struct yfFragTabStats_st {
    uint32_t                    stat_frags;
    uint32_t                    stat_seqrej;
    uint32_t                    stat_packets;
    uint32_t                    stat_dropped;
    uint32_t                    stat_peak;
};

struct yfFragTabStatsDescrip_st {
    const char    *name_frag;
    const char    *descrip_frag;
    const char    *name_seqrej;
    const char    *descrip_seqrej;
    const char    *name_packets;
    const char    *descrip_packets;
    const char    *name_dropped;
    const char    *descrip_dropped;
    const char    *name_peak;
    const char    *descrip_peak;
};

struct yfFragTabStatsDescrip_st yfFragTabStatsDescrip;

struct yfFragTab_st {
    /* State */
    uint64_t                    ctime;
    uint64_t                    prunetime;
    GHashTable                  *table;
    yfFragQueue_t               fraq;
    uint32_t                    count;
    yfFragNode_t                *assembled;
    /* Configuration */
    uint32_t                    idle_ms;
    uint32_t                    max_frags;
    /* Stats */
    struct yfFragTabStats_st    stats;
};

static uint32_t yfFragKeyHash(
    yfFragKey_t       *key)
{
    if (key->f.version == 4) {
        return key->ipid ^ (key->f.proto << 12) ^ (key->f.version << 4) ^
               key->f.addr.v4.sip ^ key->f.addr.v4.dip;
    } else {
        return key->ipid ^ (key->f.proto << 12) ^ (key->f.version << 4) ^
               *((uint32_t *)&(key->f.addr.v6.sip[0])) ^
               *((uint32_t *)&(key->f.addr.v6.sip[4])) ^
               *((uint32_t *)&(key->f.addr.v6.sip[8])) ^
               *((uint32_t *)&(key->f.addr.v6.sip[12])) ^
               *((uint32_t *)&(key->f.addr.v6.dip[0])) ^
               *((uint32_t *)&(key->f.addr.v6.dip[4])) ^
               *((uint32_t *)&(key->f.addr.v6.dip[8])) ^
               *((uint32_t *)&(key->f.addr.v6.dip[12]));
    }
}

static gboolean yfFragKeyEqual(
    yfFragKey_t       *a,
    yfFragKey_t       *b)
{
    if ((a->f.version == b->f.version) &&
        (a->ipid == b->ipid) &&
        (a->f.proto == b->f.proto)) {
        if ((a->f.version     == 4) &&
            (a->f.addr.v4.sip == b->f.addr.v4.sip) &&
            (a->f.addr.v4.dip == b->f.addr.v4.dip))
        {
            return TRUE;
        } else if ((a->f.version == 6) &&
                   (memcmp(a->f.addr.v6.sip, b->f.addr.v6.sip, 16) == 0) &&
                   (memcmp(a->f.addr.v6.dip, b->f.addr.v6.dip, 16) == 0))
        {
            return TRUE;
        } else {
            return FALSE;
        }
    } else {
        return FALSE;
    }
}

void yfGetFragTabStats(
    yfFragTab_t          *fragtab,
    uint32_t             *dropped,
    uint32_t             *assembled)
{
    *dropped = fragtab->stats.stat_dropped;
    *assembled = fragtab->stats.stat_packets;
}

static int yfFragSortByOffset(
    yfFragRec_t          *a,
    yfFragRec_t          *b)
{
    return (int)a->off - (int)b->off;
}

static void yfFragAdd(
    yfFragTab_t         *fragtab,
    yfFragNode_t        *fn,
    yfIPFragInfo_t      *fraginfo,
    uint16_t            iplen,
    const uint8_t       *pkt,
    size_t              hdr_len)
{
    yfFragRec_t         fr;
    ssize_t             frag_payoff = 0;
    ssize_t             pay_offset;

    /* FIXME figure out what of the stuff here is payload-only */

    /* append a fragment record */
    fr.off = fraginfo->offset;
    fr.len = iplen - fraginfo->iphlen;

    g_array_append_vals(fn->records, &fr, 1);

    /* changed this to accomodate rolling pcap so account for it here */
    pay_offset = fraginfo->iphlen + fn->l2info.l2hlen + fraginfo->l4hlen;

    /* set first and last flag on the fragment node */
    if (fraginfo->offset == 0) {
        fn->have_first = TRUE;
        /* first one needs to copy l2, l3, l4 (if avail) */
        frag_payoff = hdr_len > YF_FRAG_L4H_MAX?YF_FRAG_L4H_MAX :hdr_len;
    } else {
        frag_payoff = fraginfo->offset + pay_offset;
    }

    if (!fraginfo->more) {
        fn->have_last = TRUE;
    }
}

static yfFragNode_t *yfFragGetNode(
    yfFragTab_t         *fragtab,
    yfFlowKey_t         *flowkey,
    yfIPFragInfo_t      *fraginfo)
{
    yfFragNode_t        *fn;
    yfFragKey_t         fragkey;

    /* construct a key to look up the frag node */
    memcpy(&fragkey.f, flowkey, sizeof(*flowkey));
    fragkey.ipid = fraginfo->ipid;

    /* get it out of the fragment table */
    fn = g_hash_table_lookup(fragtab->table, &fragkey);
    if (fn) {
        /* and place it at the head of the fragment queue */
        piqPick(&(fragtab->fraq), fn);
        piqEnQ(&(fragtab->fraq), fn);
        fn->last_ctime = fragtab->ctime;
        return fn;
    }

    /* no fragment node available; create a new one */
    fn = yg_slice_new0(yfFragNode_t);

    /* fill in the fragment node */
    memcpy(&fn->key, &fragkey, sizeof(fragkey));

    /* allocate fragment record array */
    fn->records = g_array_sized_new(FALSE, TRUE, sizeof(yfFragRec_t), 4);

    /* create payload buffer, accounting for maximum TCP header size */
    fn->payload = yg_slice_alloc0(YF_FRAG_L4H_MAX);

    /* place it at the head of the fragment queue */
    piqEnQ(&(fragtab->fraq), fn);

    fn->last_ctime = fragtab->ctime;

    /* stick it in the fragment table */
    g_hash_table_insert(fragtab->table, &fn->key, fn);
    ++(fragtab->count);
    if (fragtab->count > fragtab->stats.stat_peak) {
        fragtab->stats.stat_peak = fragtab->count;
    }

    /* return the fragment node */
    return fn;
}

static void yfFragNodeFree(
    yfFragTab_t         *fragtab,
    yfFragNode_t        *fn)
{
    if (fn->payload) {
        yg_slice_free1(YF_FRAG_L4H_MAX, fn->payload);
    }
    if (fn->records) {
        g_array_free(fn->records, TRUE);
    }

    yg_slice_free(yfFragNode_t, fn);
}

static void yfFragRemoveNode(
    yfFragTab_t         *fragtab,
    yfFragNode_t        *fn,
    gboolean            drop)
{
    g_hash_table_remove(fragtab->table, &(fn->key));
    piqPick(&(fragtab->fraq), fn);
    --(fragtab->count);

    if (drop) {
        ++(fragtab->stats.stat_dropped);
        yfFragNodeFree(fragtab, fn);
    } else {
        ++(fragtab->stats.stat_packets);
        g_assert(fragtab->assembled == NULL);
        fragtab->assembled = fn;
    }
}

static gboolean yfFragComplete(
    yfFragTab_t         *fragtab,
    yfFragNode_t        *fn,
    yfIPFragInfo_t      *fraginfo)
{
    yfFragRec_t         *frag = NULL;
    uint32_t            i, next_off;

    /* Short circuit unless we have both the first and last fragment */
    if (!fn->have_first || !fn->have_last) {
        return FALSE;
    }

    /* Sort the fragment array by offset */
    g_array_sort(fn->records, (GCompareFunc)yfFragSortByOffset);

    /* Traverse the fragment array to see if the fragments fit */
    for (i = 0, next_off = 0; i < fn->records->len; i++) {
        frag = &g_array_index(fn->records, yfFragRec_t, i);
        if (frag->off <= next_off) {
            next_off = frag->off + frag->len;
        } else {
            /* Fragment gap. Stop. */
            return FALSE;
        }
    }

    /* If we have a short first fragment - need to do Layer 4 decode */
    if (!fn->have_l4hdr) {
        if (fn->key.f.proto == YF_PROTO_TCP) {
            if (!yfDefragTCP(fn->payload + fraginfo->iphlen+fn->l2info.l2hlen,
                             &(fn->paylen), &(fn->key.f),
                             fraginfo, &(fn->tcpinfo), &(fn->payoff)))
            {
                /* we have all fragments but still can't decode it - seeya */
                yfFragRemoveNode(fragtab, fn, TRUE);
                return FALSE;
            }
        }
    }

    /*
     * If we're here, the fragments fit. Calculate total IP length.
     * This is the total of the offsets plus the IP header.
     */
    fn->iplen = next_off + fraginfo->iphlen;

    /* Stuff the fragment in the assembled buffer. */
    yfFragRemoveNode(fragtab, fn, FALSE);

    /* we've assembled a fragment */
    return TRUE;
}

static void yfFragQueuePrune(
    yfFragTab_t     *fragtab,
    gboolean        prune_all)
{

    /* Limit prune rate */
    if (fragtab->prunetime &&
        (fragtab->ctime < fragtab->prunetime + YF_FRAGPRUNE_DELAY))
    {
        return;
    }
    fragtab->prunetime = fragtab->ctime;

    /* remove limited fragments */
    while (fragtab->max_frags &&
           fragtab->fraq.tail &&
           fragtab->count >= fragtab->max_frags)
    {
        yfFragRemoveNode(fragtab, fragtab->fraq.tail, TRUE);
    }

    /* remove expired fragments */
    while (fragtab->fraq.tail &&
           (fragtab->ctime - fragtab->fraq.tail->last_ctime > YF_FRAG_TIMEOUT))
    {
        yfFragRemoveNode(fragtab, fragtab->fraq.tail, TRUE);
    }

    /* remove all fragments */
    while (prune_all && fragtab->fraq.tail) {
        yfFragRemoveNode(fragtab, fragtab->fraq.tail, TRUE);
    }
}

yfFragTab_t *yfFragTabAlloc(
    uint32_t        idle_ms,
    uint32_t        max_frags)
{
    yfFragTab_t     *fragtab = NULL;

    /* Allocate a fragment table */
    fragtab = yg_slice_new0(yfFragTab_t);

    /* Fill in the configuration */
    fragtab->idle_ms = idle_ms;
    fragtab->max_frags = max_frags;

    /* Allocate key index table */
    fragtab->table = g_hash_table_new((GHashFunc)yfFragKeyHash,
                                      (GEqualFunc)yfFragKeyEqual);


    /* initialize the stats meta information */
    yfFragTabStatsDescrip.name_frag = "frag";
    yfFragTabStatsDescrip.descrip_frag = "frag";
    yfFragTabStatsDescrip.name_seqrej = "seqrej";
    yfFragTabStatsDescrip.descrip_seqrej = "seqrej";
    yfFragTabStatsDescrip.name_packets = "packets";
    yfFragTabStatsDescrip.descrip_packets = "packets";
    yfFragTabStatsDescrip.name_dropped = "dropped";
    yfFragTabStatsDescrip.descrip_dropped = "dropped";
    yfFragTabStatsDescrip.name_peak = "peak";
    yfFragTabStatsDescrip.descrip_peak = "peak";

    /* Done */
    return fragtab;
}

void yfFragTabFree(
    yfFragTab_t         *fragtab)
{
    while (fragtab->fraq.tail) {
        yfFragRemoveNode(fragtab, fragtab->fraq.tail, TRUE);
    }

    /* free the key index table */
    g_hash_table_destroy(fragtab->table);

    /* now free the flow table */
    yg_slice_free(yfFragTab_t, fragtab);
}

gboolean yfDefragPBuf(
    yfFragTab_t         *fragtab,
    yfIPFragInfo_t      *fraginfo,
    yfPBuf_t            *pbuf,
    const uint8_t       *pkt,
    size_t              hdrlen)
{
    yfFragNode_t        *fn;
    yfTCPInfo_t         *tcpinfo = &(pbuf->tcpinfo);
    yfL2Info_t          *l2info =  &(pbuf->l2info);
    size_t              calc_l4;

    /* short-circuit unfragmented packets */
    if (!fraginfo || !fraginfo->frag) {
        return TRUE;
    }

    /* reject out-of-sequence fragments and mark them invalid */
    if (pbuf->ptime < fragtab->ctime) {
        ++(fragtab->stats.stat_seqrej);
        pbuf->ptime = 0;
        return FALSE;
    }

    /* set fragment table packet clock */
    fragtab->ctime = pbuf->ptime;

    /* get a fragment node and place it at the head of the queue */
    fn = yfFragGetNode(fragtab, &(pbuf->key), fraginfo);

    /* stash information from first fragment */
    if (fraginfo->offset == 0) {
        if (fraginfo->l4hlen) {
            /* ports */
            fn->key.f.sp = pbuf->key.sp;
            fn->key.f.dp = pbuf->key.dp;

            /* Layer 4 info */
            fn->payoff = fraginfo->l4hlen;
            if (tcpinfo && fn->key.f.proto == YF_PROTO_TCP) {
                memcpy(&(fn->tcpinfo), tcpinfo, sizeof(*tcpinfo));
            }
            fn->have_l4hdr = 1;
        }

        /* Layer 2 info */
        if (l2info) {
            memcpy(&(fn->l2info), l2info, sizeof(*l2info));
        }
    }

    if (l2info) {
        /* this needs to be right for putting together fragmented header
           info in yfFragAdd (not sure why it would change) */
        fn->l2info.l2hlen = l2info->l2hlen;
    }

    /* add the fragment to the fragment node */
    yfFragAdd(fragtab, fn, fraginfo, pbuf->iplen, pkt, hdrlen);
    ++(fragtab->stats.stat_frags);

    calc_l4 = fn->payoff;
    /* move completed fragments to the assembled buffer */
    if (yfFragComplete(fragtab, fn, fraginfo)) {
        if (calc_l4 != fn->payoff) {
            /* defragtcp will add the tcphlen -
               add l4hlen to pbuf->allheaderlen */
            pbuf->allHeaderLen += fraginfo->l4hlen;
            fn->payoff -= fraginfo->l4hlen;
        }
    }

    /* drop expired and limited fragments off the end of the queue */
    yfFragQueuePrune(fragtab, FALSE);

    /* return and mark packet invalid if no assembled packet available */
    if (!fragtab->assembled) {
        pbuf->ptime = 0;
        return FALSE;
    }

    /* copy assembled packet into packet buffer */
    fn = fragtab->assembled;
    fragtab->assembled = NULL;

    /* Copy other values from fragment node to packet buffer */
    memcpy(&(pbuf->key), &(fn->key.f), sizeof(yfFlowKey_t));
    pbuf->iplen = fn->iplen;
    memcpy(tcpinfo, &(fn->tcpinfo), sizeof(yfTCPInfo_t));
    memcpy(l2info, &(fn->l2info), sizeof(yfL2Info_t));

    /* Free the fragment node */
    yfFragNodeFree(fragtab, fn);

    /* All done. Packet buffer is valid. */
    return TRUE;
}

void yfFragDumpStats(
    yfFragTab_t         *fragtab,
    uint64_t           packetTotal)
{
    if (!fragtab) {
        return;
    }

    g_debug("Assembled %u fragments into %u packets:",
        fragtab->stats.stat_frags, fragtab->stats.stat_packets);
    g_debug("  Expired %u incomplete fragmented packets. (%3.2f%%)",
        fragtab->stats.stat_dropped,
        ((double)(fragtab->stats.stat_dropped)/(double)(packetTotal) * 100) );
    g_debug("  Maximum fragment table size %u.",
        fragtab->stats.stat_peak);
    if (fragtab->stats.stat_seqrej) {
        g_warning("Rejected %u out-of-sequence fragments. (%3.2f%%)",
                    fragtab->stats.stat_seqrej,
                    ((double)(fragtab->stats.stat_seqrej)/(double)(packetTotal) * 100) );
    }
}
