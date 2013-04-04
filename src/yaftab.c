/**
 * @internal
 *
 ** yaftab.c
 ** YAF Active Flow Table
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2006-2012 Carnegie Mellon University. All Rights Reserved.
 ** Copyright (C) 2012      Brian Trammell.             All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell, Chris Inacio
 ** QoF redesign by Brian Trammell <brian@trammell.ch>
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
#include <airframe/logconfig.h>
#include <airframe/daeconfig.h>
#include <airframe/airutil.h>
#include <yaf/picq.h>
#include <yaf/yaftab.h>
#include <yaf/yafrag.h>

#include "qofctx.h"
#include "decode.h"

#ifndef YFDEBUG_FLOWTABLE
#define YFDEBUG_FLOWTABLE 0
#endif

#define YAF_STATE_ACTIVE        0x00000000
#define YAF_STATE_RST           0x00000001
#define YAF_STATE_FFIN          0x00000010
#define YAF_STATE_RFIN          0x00000020
#define YAF_STATE_FFINACK       0x00000040
#define YAF_STATE_RFINACK       0x00000080
#define YAF_STATE_FIN           0x000000F0
#define YAF_STATE_ATO           0x00000100

#define YF_FLUSH_DELAY 5000
#define YF_MAX_CQ      2500

typedef struct yfFlowNode_st {
    struct yfFlowNode_st        *p;
    struct yfFlowNode_st        *n;
    struct yfFlowTab_t          *flowtab;
    uint32_t                    state;
    yfFlow_t                    f;
} yfFlowNode_t;

typedef struct yfFlowQueue_st {
    yfFlowNode_t      *tail;
    yfFlowNode_t      *head;
} yfFlowQueue_t;


#if YAF_ENABLE_COMPACT_IP4
/*
 * Compact IPv4 flow structures; allows the flow table to only allocate enough
 * space for IPv4 addresses for IPv4 flows. Requires the flow key to be the
 * last element of the flow, and the flow to be the last element of the
 * flow node. ALL CHANGES made to yfFlowKey_t and yfFlow_t in yafcore.h MUST
 * be reflected here or I'll not be held responsible for the results.
 */

typedef struct yfFlowKeyIPv4_st {
    uint16_t            sp;
    uint16_t            dp;
    uint8_t             proto;
    uint8_t             version;
    uint16_t            vlanId;
#if YAF_ENABLE_DAG_SEPARATE_INTERFACES || YAF_ENABLE_NAPATECH_SEPARATE_INTERFACES || YAF_ENABLE_BIVIO
    uint8_t             netIf;
#endif
    union {
        struct {
            uint32_t    sip;
            uint32_t    dip;
        }               v4;
    }                   addr;
} yfFlowKeyIPv4_t;

typedef struct yfFlowIPv4_st {
    uint64_t        stime;
    uint64_t        etime;
    uint32_t        rdtime;
    uint8_t         reason;
    uint8_t         pcap_serial;
    uint8_t         sourceMacAddr[6];
    uint8_t         destinationMacAddr[6];

    uint8_t         pcap_file_no;
    pcap_dumper_t   *pcap;
    uint8_t         pktdir;
    yfFlowVal_t     val;
    yfFlowVal_t     rval;
    yfFlowKeyIPv4_t key;
} yfFlowIPv4_t;

typedef struct yfFlowNodeIPv4_st {
    struct yfFlowNodeIPv4_st    *p;
    struct yfFlowNodeIPv4_st    *n;
    struct yfFlowTab_t          *flowtab;
    uint32_t                    state;
    yfFlowIPv4_t                f;
} yfFlowNodeIPv4_t;

#endif

struct yfFlowTabStats_st {
    uint64_t        stat_octets;
    uint64_t        stat_packets;
    uint64_t        stat_seqrej;
    uint64_t        stat_flows;
    uint64_t        stat_uniflows;
    uint32_t        stat_peak;
    uint32_t        stat_flush;
};

struct yfFlowTab_st {
    /* State */
    uint64_t        next_fid;
    uint64_t        ctime;
    uint64_t        flushtime;
    GHashTable      *table;
    yfFlowQueue_t   aq;
    yfFlowQueue_t   cq;
    uint32_t        count;
    uint32_t        cq_count;
    /* Configuration */
    uint64_t        idle_ms;
    uint64_t        active_ms;
    uint32_t        max_flows;
    gboolean        uniflow;
    gboolean        silkmode;
    gboolean        macmode;
    gboolean        force_read_all;
    gboolean        stats_mode;
    /* Statistics */
    struct yfFlowTabStats_st stats;
};

/**
 * yfGetFlowTabStats
 *
 *
 */
void yfGetFlowTabStats(
    yfFlowTab_t *flowtab,
    uint64_t *packets,
    uint64_t *flows,
    uint64_t *rej_pkts,
    uint32_t *peak,
    uint32_t *flush)

{
    *packets = flowtab->stats.stat_packets;
    *flows = flowtab->stats.stat_flows;
    *rej_pkts = flowtab->stats.stat_seqrej;
    *peak = flowtab->stats.stat_peak;
    *flush = flowtab->stats.stat_flush;
}


/**
 * yfFlowKeyHash
 *
 * hash function that takes the 6-tuple for flow
 * identification and turns it into a single
 * 32-bit integer
 *
 * @param key pointer the the flow key which holds
 *        the set of values that uniquely identify
 *        a flow within yaf
 *
 * @return 32-bit hashed integer of the flow
 */
static uint32_t yfFlowKeyHash(
    yfFlowKey_t       *key)
{

    /* Mask out priority/CFI bits */
    uint16_t vlan_mask = 0x0FFF & key->vlanId;

#if YAF_ENABLE_DAG_SEPARATE_INTERFACES||YAF_ENABLE_NAPATECH_SEPARATE_INTERFACES
    uint32_t netInterfaceHash;

    switch (key->netIf) {
        case 0:
            netInterfaceHash = 0x33333333;
            break;
        case 1:
            netInterfaceHash = 0x55555555;
            break;
        case 2:
            netInterfaceHash = 0xaaaaaaaa;
            break;
        case 3:
            netInterfaceHash = 0xbbbbbbbb;
            break;
        default:
            /* this is impossible because of the
               dag structure is a 2-bit field for
               this */
            g_warning("invalid DAG or Napatech interface code recorded: %d"
                      " - continuing processing", key->netIf);
            netInterfaceHash = 0xcccccccc;
    }

    if (key->version == 4) {
        return (key->sp << 16) ^ key->dp ^
            (key->proto << 12) ^ (key->version << 4) ^
            (vlan_mask << 20) ^ key->addr.v4.sip ^
            key->addr.v4.dip ^ netInterfaceHash;
    } else {
        return (key->sp << 16) ^ key->dp ^
            (key->proto << 12) ^ (key->version << 4) ^
            (vlan_mask << 20) ^
            *((uint32_t *)&(key->addr.v6.sip[0])) ^
            *((uint32_t *)&(key->addr.v6.sip[4])) ^
            *((uint32_t *)&(key->addr.v6.sip[8])) ^
            *((uint32_t *)&(key->addr.v6.sip[12])) ^
            *((uint32_t *)&(key->addr.v6.dip[0])) ^
            *((uint32_t *)&(key->addr.v6.dip[4])) ^
            *((uint32_t *)&(key->addr.v6.dip[8])) ^
            *((uint32_t *)&(key->addr.v6.dip[12])) ^
            netInterfaceHash;
    }
#endif

    if (key->version == 4) {
        return (key->sp << 16) ^ key->dp ^
               (key->proto << 12) ^ (key->version << 4) ^
               (vlan_mask << 20) ^
               key->addr.v4.sip ^ key->addr.v4.dip;
    } else {
        return (key->sp << 16) ^ key->dp ^
            (key->proto << 12) ^ (key->version << 4) ^
            (vlan_mask << 20) ^
            *((uint32_t *)&(key->addr.v6.sip[0])) ^
            *((uint32_t *)&(key->addr.v6.sip[4])) ^
            *((uint32_t *)&(key->addr.v6.sip[8])) ^
            *((uint32_t *)&(key->addr.v6.sip[12])) ^
            *((uint32_t *)&(key->addr.v6.dip[0])) ^
            *((uint32_t *)&(key->addr.v6.dip[4])) ^
            *((uint32_t *)&(key->addr.v6.dip[8])) ^
            *((uint32_t *)&(key->addr.v6.dip[12]));
    }
}

/**
 * yfFlowKeyEqual
 *
 * compares two flows (a & b) based on their key value,
 * the hopefully unique 6-tuple of flow information to
 * see if the flows are the same
 *
 * @param
 *
 */
static gboolean yfFlowKeyEqual(
    yfFlowKey_t       *a,
    yfFlowKey_t       *b)
{

    uint16_t a_vlan_mask = 0x0FFF & a->vlanId;
    uint16_t b_vlan_mask = 0x0FFF & b->vlanId;

#if YAF_ENABLE_DAG_SEPARATE_INTERFACES || YAF_ENABLE_NAPATECH_SEPARATE_INTERFACES
    if (a->netIf != b->netIf) {
        return FALSE;
    }
#endif

    if ((a->sp      == b->sp)    &&
        (a->dp      == b->dp)    &&
        (a->proto   == b->proto) &&
        (a->version == b->version) &&
        (a_vlan_mask == b_vlan_mask))
    {
        if ((a->version     == 4) &&
            (a->addr.v4.sip == b->addr.v4.sip) &&
            (a->addr.v4.dip == b->addr.v4.dip))
        {
            return TRUE;
        } else if ((a->version == 6) &&
                   (memcmp(a->addr.v6.sip, b->addr.v6.sip, 16) == 0) &&
                   (memcmp(a->addr.v6.dip, b->addr.v6.dip, 16) == 0))
        {
            return TRUE;
        } else {
            return FALSE;
        }
    } else {
        return FALSE;
    }
}

/**
 * yfFlowKeyReverse
 *
 * reverses the direction of a flow key, swaping the
 * source and destination fields appropriately within
 * the key record
 *
 * @param src pointer to the forward record
 * @param dst pointer to the reversed destination record
 *
 */
static void yfFlowKeyReverse(
    yfFlowKey_t       *fwd,
    yfFlowKey_t       *rev)
{
    if (fwd->proto == YF_PROTO_ICMP || fwd->proto == YF_PROTO_ICMP6) {
        rev->sp = fwd->sp;
        rev->dp = fwd->dp;
    } else {
        rev->sp = fwd->dp;
        rev->dp = fwd->sp;
    }
    rev->proto = fwd->proto;
    rev->version = fwd->version;
    rev->vlanId = fwd->vlanId;
    if (fwd->version == 4) {
        rev->addr.v4.sip = fwd->addr.v4.dip;
        rev->addr.v4.dip = fwd->addr.v4.sip;
    } else if (fwd->version == 6) {
        memcpy(rev->addr.v6.sip, fwd->addr.v6.dip, 16);
        memcpy(rev->addr.v6.dip, fwd->addr.v6.sip, 16);
    }
#if YAF_ENABLE_DAG_SEPARATE_INTERFACES || YAF_ENABLE_NAPATECH_SEPARATE_INTERFACES
    rev->netIf = fwd->netIf;
#endif

}

/**
 * yfFlowKeyCopy
 *
 * copies a flow key from src to dst
 *
 * @param src pointer to the source flow key
 * @param dst pointer to the destination flow key
 *
 */
static void yfFlowKeyCopy(
    yfFlowKey_t       *src,
    yfFlowKey_t       *dst)
{
#if YAF_ENABLE_COMPACT_IP4
    if (src->version == 4) {
        memcpy(dst, src, sizeof(yfFlowKeyIPv4_t));
    } else {
#endif
        memcpy(dst, src, sizeof(yfFlowKey_t));
#if YAF_ENABLE_COMPACT_IP4
    }
#endif
}

/**
 *yfFlowIncrementUniflow
 *
 * simple helper function to allow counting of unidirectional flows
 * (vs. captured biflows on the wire)
 *
 */
static void
yfFlowIncrementUniflow (
    yfFlowTab_t  *flowtab)
{
    (flowtab->stats.stat_uniflows)++;
}


#if YFDEBUG_FLOWTABLE == 1
/**
 * yfFlowDebug
 *
 *
 * @param msg
 * @param flow
 *
 */
static void yfFlowDebug(
    const char        *msg,
    yfFlow_t          *flow)
{
    static GString      *str = NULL;

    if (!str) {
        str = g_string_new("");
    }

    g_string_printf(str,"%s ",msg);
    yfPrintString(str, flow);
    g_debug("%s", str->str);
}

/**
 * yfFlowTabVerifyIdleOrder
 *
 *
 * @param flowtab
 *
 */
static void yfFlowTabVerifyIdleOrder(
    yfFlowTab_t         *flowtab)
{
    yfFlowNode_t        *fn = NULL, *nfn = NULL;
    uint64_t            end;
    uint32_t            i;

    /* rip through the active queue making sure end time strictly decreases */
    for (fn = flowtab->aq.head, end = flowtab->aq.head->f.etime, i = 0;
         fn; end = fn->f.etime, fn = nfn, ++i)
    {
        nfn = fn->p;
        if (end < fn->f.etime) {
            g_debug("Flow inversion in active table position %u; "
                    "last end %llu, end %llu in flow:", i, end, fn->f.etime);
            yfFlowDebug("iiv", &(fn->f));
        }
    }
}
#endif

/**
 * yfFlowFree
 *
 * frees a flow (deallocates the memory and resets field
 * values to defaults) when the flow is no longer needed
 *
 * @param flowtab pointer to the flow table
 * @param fn node in the table to free
 *
 */
static void yfFlowFree(
    yfFlowTab_t         *flowtab,
    yfFlowNode_t        *fn)
{

    /* free dynamics if present */
    qfDynFree(&fn->f.val.tcp);
    qfDynFree(&fn->f.rval.tcp);

    /* free flow */
#if YAF_ENABLE_COMPACT_IP4
    if (fn->f.key.version == 4) {
        yg_slice_free(yfFlowNodeIPv4_t, (yfFlowNodeIPv4_t *)fn);
    } else {
#endif
        yg_slice_free(yfFlowNode_t, fn);
#if YAF_ENABLE_COMPACT_IP4
    }
#endif
}

/**
 * yfFlowTick
 *
 * advances a flow to the head of the activity
 * queue so when flows get timed out, only the
 * bottom of the queue is examined
 *
 * @param flowtable pointer to the flow table
 * @param fn pointer to the flow node entry in the
 *        table
 *
 */
static void yfFlowTick(
    yfFlowTab_t                     *flowtab,
    yfFlowNode_t                    *fn)
{
    /* move flow node to head of queue */
    if (flowtab->aq.head != fn) {
        piqPick(&flowtab->aq, fn);
        piqEnQ(&flowtab->aq, fn);
    }
}

/**
 * yfFlowClose
 *
 * close a flow and remove it from the active list, it will get flushed
 * out based on another timer; record the reason for closing the flow:
 * (time out, session end, etc.)
 *
 * @param flowtab pointer to the flow table
 * @param fn pointer to the flow node entry in the flow table
 * @param reason reason code for closing the flow
 *
 */
static void yfFlowClose(
    yfFlowTab_t                     *flowtab,
    yfFlowNode_t                    *fn,
    uint8_t                         reason)
{
    /* remove flow from table */
    g_hash_table_remove(flowtab->table, &(fn->f.key));

    /* store closure reason */
    fn->f.reason &= ~YAF_END_MASK;
    fn->f.reason |= reason;

    /* remove flow from active queue */
    piqPick(&flowtab->aq, fn);

    /* move flow node to close queue */
    piqEnQ(&flowtab->cq, fn);

    /** count the flow in the close queue */
    ++(flowtab->cq_count);

    /* count the flow as inactive */
    --(flowtab->count);
}

/**
 * yfFlowTabAlloc
 *
 * allocate (preferably from the slab allocator) another entry
 * into the flow table for a new flow
 *
 *
 * @return a pointer to the flow node entry in the flow table
 */
yfFlowTab_t *yfFlowTabAlloc(
    uint64_t        idle_ms,
    uint64_t        active_ms,
    uint32_t        max_flows,
    gboolean        uniflow,
    gboolean        silkmode,
    gboolean        macmode,
    gboolean        force_read_all,
    gboolean        stats_mode)
{
    yfFlowTab_t     *flowtab = NULL;

    /* Allocate a flow table */
    flowtab = yg_slice_new0(yfFlowTab_t);
    
    /* Set state */
    flowtab->next_fid = 1;

/* FIXME consider a better mode selection interface */

    /* Fill in the configuration */
    flowtab->idle_ms = idle_ms;
    flowtab->active_ms = active_ms;
    flowtab->max_flows = max_flows;
    flowtab->uniflow = uniflow;
    flowtab->silkmode = silkmode;
    flowtab->macmode = macmode;
    flowtab->force_read_all = force_read_all;
    flowtab->stats_mode = stats_mode;

    /* Allocate key index table */
    flowtab->table = g_hash_table_new((GHashFunc)yfFlowKeyHash,
                                      (GEqualFunc)yfFlowKeyEqual);

    /* Done */
    return flowtab;
}

/**
 * yfFlowTabFree
 *
 * free an entire flow table, including all flows therein
 *
 */
void yfFlowTabFree(
    yfFlowTab_t             *flowtab)
{
    yfFlowNode_t            *fn = NULL, *nfn = NULL;

    /* zip through the close queue freeing flows */
    for (fn = flowtab->cq.head; fn; fn = nfn) {
        nfn = fn->p;
        yfFlowFree(flowtab, fn);
    }

    /* now do the same with the active queue */
    for (fn = flowtab->aq.head; fn; fn = nfn) {
        nfn = fn->p;
        yfFlowFree(flowtab, fn);
    }

   /* free the key index table */
    g_hash_table_destroy(flowtab->table);

    /* now free the flow table */
    yg_slice_free(yfFlowTab_t, flowtab);
}

/**
 * yfFlowGetNode
 *
 * finds a flow node entry in the flow table for
 * the appropriate key value given
 *
 */
static yfFlowNode_t *yfFlowGetNode(
    yfFlowTab_t             *flowtab,
    yfFlowKey_t             *key,
    yfFlowVal_t             **valp,
    yfFlowVal_t             **rvalp,
    uint64_t                cont_fid)
{
    yfFlowKey_t             rkey;
    yfFlowNode_t            *fn;

    /* Look for flow in table */
    if ((fn = g_hash_table_lookup(flowtab->table, key))) {
        /* Forward flow found. */
        *valp = &(fn->f.val);
        *rvalp = &(fn->f.rval);
        return fn;
    }

    /* Okay. Check for reverse flow. */
    yfFlowKeyReverse(key, &rkey);
    if ((fn = g_hash_table_lookup(flowtab->table, &rkey))) {
        /* Reverse flow found. */
        *valp = &(fn->f.rval);
        *rvalp = &(fn->f.val);
        return fn;
    }

    /* Neither exists. Create a new flow and put it in the table. */
#if YAF_ENABLE_COMPACT_IP4
    if (key->version == 4) {
        fn = (yfFlowNode_t *)yg_slice_new0(yfFlowNodeIPv4_t);
    } else {
#endif
        fn = yg_slice_new0(yfFlowNode_t);
#if YAF_ENABLE_COMPACT_IP4
    }
#endif
    /* Copy key */
    yfFlowKeyCopy(key, &(fn->f.key));

    /* set flow ID */
    if (cont_fid) {
        fn->f.fid = cont_fid;
    } else {
        fn->f.fid = flowtab->next_fid++;
    }
    
    /* set flow start time */
    fn->f.stime = flowtab->ctime;

    /* set flow end time as start time */
    fn->f.etime = flowtab->ctime;

    /* stuff the flow in the table */
    g_hash_table_insert(flowtab->table, &(fn->f.key), fn);

    /* This is a forward flow */
    *valp = &(fn->f.val);
    *rvalp = &(fn->f.rval);
    
    /* Count it */
    ++(flowtab->count);
    if (flowtab->count > flowtab->stats.stat_peak) {
        flowtab->stats.stat_peak = flowtab->count;
    }

    /* All done */
    return fn;
}

/**
 * yfFlowPktIP
 *
 * process a TCP packet into the flow table specially, capture
 * all the special TCP information, flags, seq, etc.
 *
 * @param flowtab pointer to the flow table
 * @param fn pointer to the node for the relevent flow in the flow table
 * @param val
 * @param rval
 * @param tcpinfo pointer to the parsed tcp information
 *
 */

static void yfFlowPktIP( yfFlowTab_t                 *flowtab,
                         yfFlowNode_t                *fn,
                         yfFlowVal_t                 *val,
                         yfFlowVal_t                 *rval,
                         yfIPInfo_t                  *ipinfo)
{
    /* track TTL */
    if (!val->minttl || (ipinfo->ttl < val->minttl)) {
        val->minttl = ipinfo->ttl;
    }
    
    if (ipinfo->ttl > val->maxttl) {
        val->maxttl = ipinfo->ttl;
    }
    
}

/**
 * yfFlowPktTCP
 *
 * process a TCP packet into the flow table specially, capture
 * all the special TCP information, flags, seq, etc.
 *
 * @param flowtab pointer to the flow table
 * @param fn pointer to the node for the relevent flow in the flow table
 * @param val
 * @param rval
 * @param tcpinfo pointer to the parsed tcp information
 *
 */

static void yfFlowPktTCP(
    yfFlowTab_t                 *flowtab,
    yfFlowNode_t                *fn,
    yfFlowVal_t                 *val,
    yfFlowVal_t                 *rval,
    yfTCPInfo_t                 *tcpinfo,
    size_t                      datalen)
{
    
    /* handle flags */
    if (!val->pkt) {
        /* Not the first packet. Union flags. */
        val->uflags |= tcpinfo->flags;
    } else {
        /* First packet. Initial flags. */
        val->iflags = tcpinfo->flags;
    }

    /* track tcp dynamics */
    qfDynSeq(&val->tcp, tcpinfo->seq, (uint32_t)datalen,
             (uint32_t)(UINT32_MAX & flowtab->ctime));
    if (tcpinfo->flags & YF_TF_ACK) {
        qfDynAck(&rval->tcp, tcpinfo->ack,
                 (uint32_t)(UINT32_MAX & flowtab->ctime));
    }
    
    /* Store information from options */
    if (tcpinfo->mss) fn->f.val.tcp.mss_opt = tcpinfo->mss;
    
    /* Update flow state for FIN flag */
    if (val == &(fn->f.val)) {
        if (tcpinfo->flags & YF_TF_FIN)
            fn->state |= YAF_STATE_FFIN;
        if ((fn->state & YAF_STATE_RFIN) && (tcpinfo->flags & YF_TF_ACK))
            fn->state |= YAF_STATE_FFINACK;
    } else {
        if (tcpinfo->flags & YF_TF_FIN)
            fn->state |= YAF_STATE_RFIN;
        if ((fn->state & YAF_STATE_FFIN) && (tcpinfo->flags & YF_TF_ACK))
            fn->state |= YAF_STATE_RFINACK;
    }

    /* Update flow state for RST flag */
    if (tcpinfo->flags & YF_TF_RST) {
        fn->state |= YAF_STATE_RST;
    }
}

/**
 * yfFlowPBuf
 *
 * parse a packet buffer structure and turn it into a flow record
 * this may update an existing flow record, or get a new flow record
 * and populate it.  It calls various functions to decode the
 * packet buffer and extract protocol details
 *
 * @param flowtab pointer to the flow table
 * @param pbuflen length of the packet buffer
 * @param pbuf pointer to the packet data
 *
 */
void yfFlowPBuf(
    yfFlowTab_t                 *flowtab,
    yfPBuf_t                    *pbuf)
{
    yfFlowKey_t                 *key = &(pbuf->key);
    // FIXME unused variable -- delete?
    //    yfFlowKey_t                 rkey;
    yfFlowVal_t                 *val = NULL;
    yfFlowVal_t                 *rval = NULL;
    yfFlowNode_t                *fn = NULL;
    yfIPInfo_t                  *ipinfo = &(pbuf->ipinfo);
    yfTCPInfo_t                 *tcpinfo = &(pbuf->tcpinfo);
    yfL2Info_t                  *l2info = &(pbuf->l2info);
    uint16_t                    datalen = (pbuf->iplen - pbuf->allHeaderLen +
                                           l2info->l2hlen);
    uint64_t                    cont_fid = 0;

    /* skip and count out of sequence packets */
    if (pbuf->ptime < flowtab->ctime) {
        if (!flowtab->force_read_all) {
            ++(flowtab->stats.stat_seqrej);
            return;
        } else {
            fprintf(stderr, "Skipping out of sequence packet until yfAddOufOfSequence refactor complete.");
            ++(flowtab->stats.stat_seqrej);
            /* yfAddOutOfSequence(flowtab, key, pbuflen, pbuf); */
            return;
        }
    }

    /* update flow table current time */
    flowtab->ctime = pbuf->ptime;

    /* Count the packet and its octets */
    ++(flowtab->stats.stat_packets);
    flowtab->stats.stat_octets += pbuf->iplen;

    /* Get a flow node for this flow */
    fn = yfFlowGetNode(flowtab, key, &val, &rval, 0);

    /* Check for active timeout or counter overflow */
    if (((pbuf->ptime - fn->f.stime) > flowtab->active_ms) ||
        (flowtab->silkmode && (val->oct + pbuf->iplen > UINT32_MAX)))
    {
        cont_fid = fn->f.fid;
        yfFlowClose(flowtab, fn, YAF_END_ACTIVE);
        /* get a new flow node containing this packet */
        fn = yfFlowGetNode(flowtab, key, &val, &rval, cont_fid);
        /* set continuation flag in silk mode */
        if (flowtab->silkmode) fn->f.reason = YAF_ENDF_ISCONT;
    }

    /* Check for inactive timeout - esp when reading from pcap */
    if ((pbuf->ptime - fn->f.etime) > flowtab->idle_ms) {
        cont_fid = fn->f.fid;
        yfFlowClose(flowtab, fn, YAF_END_IDLE);
        /* get a new flow node for the current packet */
        fn = yfFlowGetNode(flowtab, key, &val, &rval, cont_fid);
    }

    /* Calculate reverse SYN/ACK RTT */
    if (val->pkt == 0 && val == &(fn->f.rval)) {
        fn->f.rdtime = (uint32_t)(pbuf->ptime - fn->f.stime);
    }

    /* Do IP stuff */
    yfFlowPktIP(flowtab, fn, val, rval, ipinfo);
    
    /* Do TCP stuff */
    if (fn->f.key.proto == YF_PROTO_TCP) {
        /* Handle TCP flows specially (flags, ISN, sequenced payload) */
        yfFlowPktTCP(flowtab, fn, val, rval, tcpinfo, datalen);
    } 

    if (val->pkt == 0) {
        if (flowtab->macmode && val == &(fn->f.val)) {
            /* Note Mac Addr */
            if (l2info) {
                memcpy(fn->f.sourceMacAddr, l2info->smac,
                       ETHERNET_MAC_ADDR_LENGTH);
                memcpy(fn->f.destinationMacAddr, l2info->dmac,
                       ETHERNET_MAC_ADDR_LENGTH);
            }
        }
//        val->first_pkt_size = pbuf->iplen;
    } else {
        /* compare packet sizes */
//        if ( pbuf->iplen == val->first_pkt_size ) {
//            if (val->pkt == 1) {
//                val->attributes = YAF_SAME_SIZE;
//            }
//        } else {
//            val->attributes = 0;
//        }
    }

#if YAF_ENABLE_BIVIO
    val->netIf = pbuf->key.netIf;
#endif

    /* Count packets and octets */
    val->oct += pbuf->iplen;
    val->appoct += datalen;

    val->pkt += 1;
    if (datalen) val->apppkt += 1;
    
    /* update flow end time */
    fn->f.etime = pbuf->ptime;

    /* Update stats */
    // FIXME removed for now, moving all stats down into the values
    // first up, appoct
    // if (flowtab->stats_mode) {
    //     yfFlowStatistics(fn, val, pbuf->ptime, datalen);
    // }

    /* close flow, or move it to head of queue */
    if ((fn->state & YAF_STATE_FIN) == YAF_STATE_FIN ||
        fn->state & YAF_STATE_RST)
    {
        yfFlowClose(flowtab, fn, YAF_END_CLOSED);
    } else {
        yfFlowTick(flowtab, fn);
    }
}

/**
 * yfUniflow
 *
 * creates a uniflow record from a biflow record, in order to split
 * the record into a single record for uniflow only collection systems
 *
 * @param bf pointer to normal biflow yaf flow record
 * @param uf pointer to a new flow record, that will have its rev
 *           (reverse) values NULLed
 *
 */
static void yfUniflow(
    yfFlow_t          *bf,
    yfFlow_t          *uf)
{

#if YAF_ENABLE_COMPACT_IP4
    if (bf->key.version == 4) {
        memcpy(uf, bf, sizeof(yfFlowIPv4_t));
    } else {
#endif
        memcpy(uf, bf, sizeof(yfFlow_t));
#if YAF_ENABLE_COMPACT_IP4
    }
#endif
    memset(&(uf->rval), 0, sizeof(yfFlowVal_t));
    uf->rdtime = 0;
}

/**
 * yfUniflowReverse
 *
 * reverses the flow information in the biflow in order to generate
 * two uniflow outputs
 *
 *
 * @param bf pointer to biflow record
 * @param uf pointer to uniflow record to fill in
 *
 * @return TRUE on success, FALSE on error
 */
static gboolean yfUniflowReverse(
    yfFlow_t          *bf,
    yfFlow_t          *uf)
{
    if (!(bf->rval.pkt)) return FALSE;

    /* calculate reverse time */
    uf->stime = bf->stime + bf->rdtime;
    uf->etime = bf->etime;
    uf->rdtime = 0;
    if (bf->destinationMacAddr) {
        memcpy(uf->sourceMacAddr, bf->destinationMacAddr,
               ETHERNET_MAC_ADDR_LENGTH);
    }
    if (bf->sourceMacAddr) {
        memcpy(uf->destinationMacAddr, bf->sourceMacAddr,
               ETHERNET_MAC_ADDR_LENGTH);
    }
    /* reverse key */
    yfFlowKeyReverse(&bf->key, &uf->key);

    /* copy and reverse value */
    memcpy(&(uf->val), &(bf->rval), sizeof(yfFlowVal_t));
    memset(&(uf->rval), 0, sizeof(yfFlowVal_t));

    /* copy reason */
    uf->reason = bf->reason;

    /* all done */
    return TRUE;
}

/**
 * yfFlowTabFlush
 *
 *
 *
 */
gboolean yfFlowTabFlush(
    void            *yfContext,
    gboolean        close,
    GError          **err)
{
    gboolean        wok = TRUE;
    yfFlowNode_t    *fn = NULL;
    yfFlow_t        uf;
    yfContext_t     *ctx = (yfContext_t *)yfContext;
    yfFlowTab_t     *flowtab = ctx->flowtab;

    if (!close && flowtab->flushtime &&
        (flowtab->ctime < flowtab->flushtime + YF_FLUSH_DELAY)
        && (flowtab->cq_count < YF_MAX_CQ))
    {
        return TRUE;
    }

    flowtab->flushtime = flowtab->ctime;

    /* Count the flush */
    ++flowtab->stats.stat_flush;

    /* Verify flow table order */
    /* yfFlowTabVerifyIdleOrder(flowtab);*/
    /* close idle flows */
    while (flowtab->aq.tail &&
           (flowtab->ctime - flowtab->aq.tail->f.etime > flowtab->idle_ms))
    {
        yfFlowClose(flowtab, flowtab->aq.tail, YAF_END_IDLE);
    }

    /* close limited flows */
    while (flowtab->max_flows &&
           flowtab->aq.tail &&
           flowtab->count >= flowtab->max_flows)
    {
        yfFlowClose(flowtab, flowtab->aq.tail, YAF_END_RESOURCE);
    }

    /* close all flows if flushing all */
    while (close && flowtab->aq.tail) {
        yfFlowClose(flowtab, flowtab->aq.tail, YAF_END_FORCED);
    }

    /* flush flows from close queue */
    while ((fn = piqDeQ(&flowtab->cq))) {
        /* quick accounting of asymmetric/uniflow records present */
        if ((fn->f.rval.oct == 0) && (fn->f.rval.pkt == 0)) {
            ++(flowtab->stats.stat_uniflows);
        }
        /* write flow */
        if (flowtab->uniflow) {
            /* Uniflow mode. Split flow in two and write. */
            yfUniflow(&(fn->f), &uf);
            wok = yfWriteFlow(ctx, &uf, err);
            if (wok) {
                ++(flowtab->stats.stat_flows);
            }
            if (wok && yfUniflowReverse(&(fn->f), &uf)) {
                wok = yfWriteFlow(ctx, &uf, err);
                if (wok) {
                    ++(flowtab->stats.stat_flows);
                }
            }
        } else {
            /* Biflow mode. Write flow whole. */
            wok = yfWriteFlow(ctx, &(fn->f), err);
            if (wok) {
                ++(flowtab->stats.stat_flows);
            }
        }
        --(flowtab->cq_count);

        /* free it */
        yfFlowFree(flowtab, fn);

        /* return error if necessary */
        if (!wok) return wok;
    }

    return TRUE;
}

/**
 * yfFlowTabCurrentTime
 *
 *
 *
 *
 */
uint64_t yfFlowTabCurrentTime(
    yfFlowTab_t     *flowtab)
{
    return flowtab->ctime;
}


/**
 * yfFlowDumpStats
 *
 * prints out statistics about flow, packet rates along with some
 * internal diagnostic type statistics as requested
 *
 *
 * @param flowtab pointer to the flow table
 * @param timer a glib timer to calculate rates for the flow table
 *
 *
 */
uint64_t yfFlowDumpStats(
    yfFlowTab_t     *flowtab,
    GTimer          *timer)
{
    g_debug("Processed %llu packets into %llu flows:",
            (long long unsigned int)flowtab->stats.stat_packets,
            (long long unsigned int)flowtab->stats.stat_flows);
    if (timer) {
        g_debug("  Mean flow rate %.2f/s.",
                ((double)flowtab->stats.stat_flows / g_timer_elapsed(timer, NULL)));
        g_debug("  Mean packet rate %.2f/s.",
                ((double)flowtab->stats.stat_packets / g_timer_elapsed(timer, NULL)));
        g_debug("  Virtual bandwidth %.4f Mbps.",
                ((((double)flowtab->stats.stat_octets * 8.0) / 1000000) /
                 g_timer_elapsed(timer, NULL)));
    }
    g_debug("  Maximum flow table size %u.", flowtab->stats.stat_peak);
    g_debug("  %u flush events.", flowtab->stats.stat_flush);
    if (flowtab->stats.stat_seqrej) {
        g_warning("Rejected %"PRIu64" out-of-sequence packets.",
                  flowtab->stats.stat_seqrej);
    }
    g_debug("  %"PRIu64" asymmetric/unidirectional flows detected (%2.2f%%)",
            flowtab->stats.stat_uniflows,
            (((double)flowtab->stats.stat_uniflows)/((double)flowtab->stats.stat_flows)) * 100);

    return flowtab->stats.stat_packets;
}

/*
 * Code Graveyard 
 */

/* maybe bring this back, but for now move stuff directly into the values... */
#if 0
static void
yfFlowStatistics(
                 yfFlowNode_t            *fn,
                 yfFlowVal_t             *val,
                 uint64_t                ptime,
                 uint16_t                datalen)
{
    
    if (val->stats.ltime) {
        val->stats.aitime += (ptime - val->stats.ltime);
    }
    
    if (val->pkt > 1 && val->pkt < 12) {
        val->stats.iaarray[val->pkt -2] = (ptime - val->stats.ltime);
    }
    
    val->stats.ltime = fn->f.etime;
    
    if (datalen) {
        /* that means there is some payload */
        if (val == &(fn->f.rval)) {
            fn->f.pktdir |= (1 << (fn->f.val.stats.nonemptypktct +
                                   val->stats.nonemptypktct));
        }
        if (val->stats.nonemptypktct < 10) {
            val->stats.pktsize[val->stats.nonemptypktct] = datalen;
        }
        val->stats.nonemptypktct++;
        if (datalen < 60) {
            val->stats.smallpktct++;
        } else if (datalen > 225) {
            val->stats.largepktct++;
        }
        val->stats.payoct += datalen;
        if (val->stats.firstpktsize== 0) {
            val->stats.firstpktsize= datalen;
        }
        if (datalen> val->stats.maxpktsize) {
            val->stats.maxpktsize =datalen;
        }
    }
    
}
#endif

#if 0
static void
yfAddOutOfSequence(
                   yfFlowTab_t             *flowtab,
                   yfFlowKey_t             *key,
                   size_t                  pbuflen,
                   yfPBuf_t                *pbuf)
{
    yfFlowNode_t            *fn = NULL;
    yfFlowNode_t            *tn = NULL;
    yfFlowNode_t            *nfn = NULL;
    yfFlowKey_t             rkey;
    uint64_t                end;
    yfFlowVal_t             *val = NULL;
    yfTCPInfo_t             *tcpinfo = &(pbuf->tcpinfo);
    yfL2Info_t              *l2info = &(pbuf->l2info);
    uint8_t                 *payload = (pbuflen >= YF_PBUFLEN_BASE) ?
    pbuf->payload : NULL;
    size_t                  paylen = (pbuflen >= YF_PBUFLEN_BASE) ?
    pbuf->paylen : 0;
    uint16_t                datalen = (pbuf->paylen - pbuf->allHeaderLen +
                                       l2info->l2hlen);
    uint32_t                pcap_len = 0;
    uint32_t                hash;
    gboolean                rev = FALSE;
    
    /* Count the packet and its octets */
    ++(flowtab->stats.stat_packets);
    flowtab->stats.stat_octets += pbuf->iplen;
    
    if (payload) {
        if (paylen >= pbuf->allHeaderLen) {
            paylen -= pbuf->allHeaderLen;
            payload += pbuf->allHeaderLen;
        } else {
            paylen = 0;
            payload = NULL;
        }
    }
    
    /* Look for flow in table */
    if ((fn = g_hash_table_lookup(flowtab->table, key))) {
        /* Forward flow found. */
        val = &(fn->f.val);
    }
    
    if (fn == NULL) {
        /* Okay. Check for reverse flow. */
        yfFlowKeyReverse(key, &rkey);
        rev = TRUE;
        if ((fn = g_hash_table_lookup(flowtab->table, &rkey))) {
            /* Reverse flow found. */
            val = &(fn->f.rval);
        }
    }
    
    if (fn == NULL) {
        /* Neither exists. Create a new flow and put it in the table. */
#if YAF_ENABLE_COMPACT_IP4
        if (key->version == 4) {
            fn = (yfFlowNode_t *)yg_slice_new0(yfFlowNodeIPv4_t);
        } else {
#endif
            fn = yg_slice_new0(yfFlowNode_t);
#if YAF_ENABLE_COMPACT_IP4
        }
#endif
        /* Copy key */
        yfFlowKeyCopy(key, &(fn->f.key));
        
        /* set flow start time */
        fn->f.stime = pbuf->ptime;
        
        /* set flow end time as start time */
        fn->f.etime = pbuf->ptime;
        
        /* stuff the flow in the table */
        g_hash_table_insert(flowtab->table, &(fn->f.key), fn);
        
        /* This is a forward flow */
        val = &(fn->f.val);
        
        /* Count it */
        ++(flowtab->count);
        if (flowtab->count > flowtab->stats.stat_peak) {
            flowtab->stats.stat_peak = flowtab->count;
        }
        
    }
    /* packet exists now, update info */
    
    /* Do payload and TCP stuff */
    if (fn->f.key.proto == YF_PROTO_TCP) {
        /* Handle TCP flows specially (flags, ISN, sequenced payload) */
        yfFlowPktTCP(flowtab, fn, val, payload, paylen, tcpinfo);
        
    }
    
    if (val->pkt == 0) {
        val->first_pkt_size = pbuf->iplen;
        /* Note Mac Addr */
        if (flowtab->macmode && (val == &(fn->f.val))) {
            if (l2info) {
                memcpy(fn->f.sourceMacAddr, l2info->smac,
                       ETHERNET_MAC_ADDR_LENGTH);
                memcpy(fn->f.destinationMacAddr, l2info->dmac,
                       ETHERNET_MAC_ADDR_LENGTH);
            }
        }
    } else {
        /* compare packet sizes */
        if ( pbuf->iplen == val->first_pkt_size ) {
            if (val->pkt == 1) {
                val->attributes = YAF_SAME_SIZE;
            }
        } else {
            val->attributes &= 0xFE;
        }
    }
    
    /* set flow attributes - this flow is out of order */
    val->attributes |= YAF_OUT_OF_SEQUENCE;
    
    /* Count packets and octets */
    val->oct += pbuf->iplen;
    val->pkt += 1;
    
    /* don't update end time - stime could be greater than etime */
    
    /* Update stats */
    if (flowtab->stats_mode) {
        yfFlowStatistics(fn, val, pbuf->ptime, datalen);
    }
    
    
    pcap_len = pbuf->pcap_hdr.caplen + 16;
    
    /* close flow, or move it to head of queue */
    if ((fn->state & YAF_STATE_FIN) == YAF_STATE_FIN ||
        fn->state & YAF_STATE_RST)
    {
        yfFlowClose(flowtab, fn, YAF_END_CLOSED);
        return;
    }
    
    /* Check for inactive timeout - this flow might be idled out on arrival */
    if ((flowtab->ctime - pbuf->ptime) > flowtab->idle_ms) {
        yfFlowClose(flowtab, fn, YAF_END_IDLE);
        return;
    }
    
    if (flowtab->aq.head == NULL) {
        yfFlowTick(flowtab, fn);
        return;
    }
    
    /* rip through the active queue and put this in the right spot */
    /* first remove the node */
    piqPick(&flowtab->aq, fn);
    
    for (tn = flowtab->aq.head; tn; tn=nfn)
    {
        end = tn->f.etime;
        nfn = tn->p;
        if (end <= fn->f.etime) {
            /* nfn is previous node */
            nfn = tn->n;
            /* point previous (next) to new node */
            nfn->p = fn;
            /* point current previous to new node */
            tn->n = fn;
            /* point new node's next to current */
            fn->p = tn;
            /* point new node's previous to previous */
            fn->n = nfn;
            /*yfFlowTabVerifyIdleOrder(flowtab);*/
            return;
        }
    }
    
    /* if this happens, we are at the tail */
    if (flowtab->aq.tail) {
        nfn = flowtab->aq.tail;
        /* this flow's next (in non-Brian land - previous) is the tail */
        fn->n = nfn;
        /* the tail's previous (next) now points to new node */
        nfn->p = fn;
        /* tail is now new node */
        flowtab->aq.tail = fn;
    } else {
        /* shouldn't get here but if we do,just get rid of this troublemaker.*/
        yfFlowClose(flowtab, fn, YAF_END_IDLE);
    }
}
#endif
