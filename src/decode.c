/**
 ** decode.c
 ** YAF Layer 2 and Layer 3 decode routines
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2007-2012 Carnegie Mellon University. All Rights Reserved.
 ** Copyright (C) 2012-2013 Brian Trammell.             All Rights Reserved
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell <brian@trammell.ch>
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
#include <yaf/decode.h>
#include <airframe/airutil.h>

/* Definitions of the various headers the decoder understands */

typedef struct yfHdrEn10Mb_st {
    uint8_t         dmac[6];
    uint8_t         smac[6];
    uint16_t        type;
} yfHdrEn10Mb_t;

typedef struct yfHdrChdlc_st {
    uint8_t         address;
    uint8_t         control;
    uint16_t        type;
} yfHdrChdlc_t;

typedef struct yfHdrLinuxSll_st {
    uint16_t        sll_type;
    uint16_t        addr_type;
    uint16_t        addr_len;
    uint8_t         addr[8];
    uint16_t        type;
} yfHdrLinuxSll_t;

typedef struct yfHdrPppOeShim_st {
    uint8_t         vertype;
    uint8_t         code;
    uint16_t        session;
    uint16_t        length;
} yfHdrPppOeShim_t;

typedef struct yfHdr1qShim_st {
    uint16_t        ptt;
    uint16_t        type;
} yfHdr1qShim_t;

typedef struct yfHdrJuniper_st {
    uint8_t         magic[3];
    uint8_t         flags;
    uint16_t        ext_len;
} yfHdrJuniper_t;

typedef struct yfHdrNull_st {
    uint32_t        addr_family;
} yfHdrNull_t;

/** Ethertype for 802.1q VLAN shim header */
#define YF_TYPE_8021Q   0x8100
/** Ethertype for MPLS unicast shim header */
#define YF_TYPE_MPLS    0x8847
/** Ethertype for MPLE multicast shim header */
#define YF_TYPE_MPLSM   0x8848
/** Ethertype for PPPoE shim header */
#define YF_TYPE_PPPOE   0x8864
/** Ethertype for ARP */
#define YF_TYPE_ARP     0x0806

/** Ethernet encoding types:
0x0800  IP v4
0x0806  ARP
0x8035  RARP
0x809b  ApppleTalk
0x80f3  AppleTalk ARP
0x8100  802.1Q tag
0x8137  Novell IPX (alternate)
0x8138  Novell
0x86dd  IP v6
0x8847  MPLS unicast
0x8848  MPLS multicast
0x8863  PPPoE discovery
0x8864  PPPoE session
*/

/** PPP type for IPv4 */
#define YF_PPPTYPE_IPv4 0x0021
/** PPP type for IPv6 */
#define YF_PPPTYPE_IPv6 0x0057
/** PPP type for MPLS unicast shim header */
#define YF_PPPTYPE_MPLS 0x0281
/** PPP type for IPv6 */
#define YF_PPPTYPE_MPLSM 0x0283

/* 802.1q VLAN tag decode macros */

#define YF_VLAN_TAG(_pkt_) (0x0FFF &(g_ntohs(((yfHdr1qShim_t *)(_pkt_))->ptt)))

/* MPLS label decode macros */
#define YF_MPLS_LABEL(_x_) (((_x_) & 0xFFFFF000) >> 12)
#define YF_MPLS_EXP(_x_)   (((_x_) & 0x00000E00) >> 9)
#define YF_MPLS_LAST(_x_)   ((_x_) & 0x00000100)
#define YF_MPLS_TTL(_x_)    ((_x_) & 0x000000FF)

/* Juniper flags */
#define JUNIPER_PKT_OUT   0x00
#define JUNIPER_PKT_IN    0x01
#define JUNIPER_NO_L2     0x02
#define JUNIPER_FLAG_EXT  0x80
#define JUNIPER_MAGIC     0x4d4743

/* IP v4/v6 version macros */
#define YF_IP_VERSION(_pkt_)  ((*((uint8_t *)(_pkt_)) & 0xF0) >> 4)

#define YF_IP_VERSION_TO_TYPE(_pkt_, _caplen_, _type_) {    \
    uint8_t         _ipv;                                   \
    /* Check for at least one byte for IP version */        \
    if ((_caplen_) < 1) return NULL;                        \
    /* Fake ethertype based upon IP version */              \
    _ipv = YF_IP_VERSION(_pkt_);                            \
    if (_ipv == 4) {                                        \
        (_type_) = YF_TYPE_IPv4;                            \
    } else if (_ipv == 6) {                                 \
        (_type_) = YF_TYPE_IPv6;                            \
    } else {                                                \
        return NULL;                                        \
    }                                                       \
}

/**
 * IPv4 header structure, without options.
 */
typedef struct yfHdrIPv4_st{
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    /** IP header length in 32-bit words. */
    unsigned int    ip_hl:4,
    /** IP version. Always 4 for IPv4 packets.*/
                    ip_v:4;
#elif G_BYTE_ORDER == G_BIG_ENDIAN
    /** IP version. Always 4 for IPv4 packets.*/
    unsigned int    ip_v:4,
    /** IP header length in 32-bit words. */
                    ip_hl:4;
#else
#error Cannot determine byte order while defining IP header structure.
#endif
    /** Type of Service */
    uint8_t         ip_tos;
    /** Total IP datagram length including header in bytes */
    uint16_t        ip_len;
    /** Fragment identifier */
    uint16_t        ip_id;
    /** Fragment offset and flags */
    uint16_t        ip_off;
    /** Time to live in routing hops */
    uint8_t         ip_ttl;
    /** Protocol identifier */
    uint8_t         ip_p;
    /** Header checksum */
    uint16_t        ip_sum;
    /** Source IPv4 address */
    uint32_t        ip_src;
    /** Destination IPv4 address */
    uint32_t        ip_dst;
} yfHdrIPv4_t;

/** IPv4 don't fragment flag. For decoding yfHdrIPv4_t.ip_off. */
#define    YF_IP4_DF 0x4000
/** IPv4 more fragments flag. For decoding yfHdrIPv4_t.ip_off. */
#define    YF_IP4_MF 0x2000
/** IPv4 fragment offset mask. For decoding yfHdrIPv4_t.ip_off. */
#define    YF_IP4_OFFMASK 0x1fff

/**
 * IPv6 header structure.
 */
typedef struct yfHdrIPv6_st {
    /** Version, traffic class, and flow ID. Use YF_VCF6_ macros to access. */
    uint32_t        ip6_vcf;
    /**
     * Payload length. Does NOT include IPv6 header (40 bytes), but does
     * include subsequent extension headers, upper layer headers, and payload.
     */
    uint16_t        ip6_plen;
    /** Next header identifier. Use YF_PROTO_ macros. */
    uint8_t         ip6_nxt;
    /** Hop limit */
    uint8_t         ip6_hlim;
    /** Source IPv6 address */
    uint8_t         ip6_src[16];
    /** Destination IPv6 address */
    uint8_t         ip6_dst[16];
} yfHdrIPv6_t;

/* Version, class, and flow decode macros */
#define YF_VCF6_VERSION(_ip6hdr_)   (((_ip6hdr_)->ip6_vcf & 0xF0000000) >> 28)
#define YF_VCF6_CLASS(_ip6hdr_)     (((_ip6hdr_)->ip6_vcf & 0x0FF00000) >> 20)
#define YF_VCF6_ECN(_ip6hdr_)       (((_ip6hdr_)->ip6_vcf & 0x0C000000) >> 26)
#define YF_VCF6_FLOW(_ip6hdr_)       ((_ip6hdr_)->ip6_vcf & 0x000FFFFF)

/**
 * IPv6 partial extension header structure. Used to decode next and length only.
 */

typedef struct yfHdrIPv6Ext_st {
    /** Next header identifier. Use YF_PROTO_ macros. */
    uint8_t     ip6e_nxt;
    /** Extension header length. */
    uint8_t     ip6e_len;
} yfHdrIPv6Ext_t;

/**
 * IPv6 fragment extension header structure.
 */
typedef struct yfHdrIPv6Frag_st  {
    /** Next header identifier. Use YF_PROTO_ macros. */
    uint8_t     ip6f_nxt;
    /** Reserved field. */
    uint8_t     ip6f_reserved;
    /** Fragment offset and flags. */
    uint16_t    ip6f_offlg;
    /** Fragment identifier. */
    uint32_t    ip6f_ident;
} yfHdrIPv6Frag_t;

/* IPv6 Fragmentation decode macros */
#define YF_IP6_MF       0x0001
#define YF_IP6_OFFMASK  0xfff8

/**
 * TCP header structure, without options.
 */
typedef struct yfHdrTcp_st {
    /** Source port */
    uint16_t        th_sport;
    /** Destination port */
    uint16_t        th_dport;
    /** Sequence number */
    uint32_t        th_seq;
    /** Acknowledgment number */
    uint32_t        th_ack;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    /** Unused. Must be 0. */
    unsigned int    th_x2:4,
    /** Data offset. TCP header length in 32-bit words. */
                    th_off:4;
#elif G_BYTE_ORDER == G_BIG_ENDIAN
    /** Data offset. TCP header length in 32-bit words. */
    unsigned int    th_off:4,
    /** Unused. Must be 0. */
                    th_x2:4;
#else
#error Cannot determine byte order while defining TCP header structure.
#endif
    /** TCP flags. */
    uint8_t         th_flags;
    /** Congestion window. */
    uint16_t        th_win;
    /** Segment checksum. */
    uint16_t        th_sum;
    /** Urgent pointer. */
    uint16_t        th_urp;
} yfHdrTcp_t;

/** Length of a TCP header without options */
#define YF_TCP_HLEN 20

typedef struct yfHdrTcpOptTs_st {
    uint32_t         ts_val;
    uint32_t         ts_ecr;
} yfHdrTcpOptTs_t;

#define YF_TOK_EOL      0
#define YF_TOK_NOP      1
#define YF_TOK_MSS      2
#define YF_TOK_WS       3
#define YF_TOK_SACKOK   4
#define YF_TOK_SACK     5
#define YF_TOK_TS       8

/**
 * UDP header structure.
 */
typedef struct yfHdrUdp_st {
    /** Source port */
    uint16_t        uh_sport;
    /** Destination port */
    uint16_t        uh_dport;
    /** UDP length. Includes header and payload, in octets. */
    uint16_t        uh_ulen;
    /** UDP checksum. Calculated over the entire message. */
    uint16_t        uh_sum;
} yfHdrUdp_t;

/**
 * ICMP/ICMP6 partial header structure. Used to decode type and code only.
 */
typedef struct ydHdrIcmp_st {
    /* ICMP type */
    u_char    icmp_type;
    /* ICMP code */
    u_char    icmp_code;
} yfHdrIcmp_t;

/**
 * GRE partial header structure. Used to decode the first 4 (fixed) bytes
 * of the GRE header only.
 */
typedef struct yfHdrGre_st {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    /** Recursion Control */
     unsigned int   gh_recur:3,
    /** Strict Source Routing */
                    gh_f_ssr:1,
    /** Sequence Number Present */
                    gh_f_seq:1,
    /** Key Present */
                    gh_f_key:1,
    /** Routing Present */
                    gh_f_route:1,
    /** Checksum Present */
                    gh_f_sum:1;
#elif G_BYTE_ORDER == G_BIG_ENDIAN
    /** Checksum Present */
    unsigned int    gh_f_sum:1,
    /** Routing Present */
                    gh_f_route:1,
    /** Key Present */
                    gh_f_key:1,
    /** Sequence Number Present */
                    gh_f_seq:1,
    /** Strict Source Routing */
                    gh_f_ssr:1,
    /** Recursion Control */
                    gh_recur:3;
#else
#error Cannot determine byte order while defining GRE header structure.
#endif
    /** Flags and Version. Reserved, must be zero */
    uint8_t         gh_fv;
    /** Protocol. Ethertype of next header. */
    uint16_t        gh_type;
} yfHdrGre_t;

/* Version, class, and flow decode macros */
#define YF_GHFV_VERSION(_grehdr_)   ((_grehdr_)->gh_fv & 0x07)

/**
 * GRE Source Route Entry partial structure. Used to decode the first 4
 * (fixed) bytes of the SRE only.
 */
typedef struct yfHdrSre_st {
    /** Address family for routing information */
    uint16_t        gh_sre_af;
    /** SRE offset */
    uint8_t         gh_sre_off;
    /** SRE length */
    uint8_t         gh_sre_len;
} yfHdrSre_t;

/* Decode context for configuration and statistics */
struct yfDecodeCtx_st {
    /* State (none) */
    /* Configuration */
    int             datalink;
    uint16_t        reqtype;
    gboolean        gremode;
    /* Statistics */
    struct stats_tag {
        uint32_t        fail_l2hdr;
        uint32_t        fail_l2shim;
        uint32_t        fail_l2loop;
        uint32_t        fail_l3type;
        uint32_t        fail_arptype;
        uint32_t        fail_ip4hdr;
        uint32_t        fail_ip4frag;
        uint32_t        fail_ip6hdr;
        uint32_t        fail_ip6ext;
        uint32_t        fail_ip6frag;
        uint32_t        fail_l4hdr;
        uint32_t        fail_l4frag;
        uint32_t        fail_grevers;
    } stats;
};

/**
 * yfDecodeSetLinktype
 */

#if YAF_ENABLE_LIBTRACE
void yfDecodeSetLinktype(
    yfDecodeCtx_t           *ctx,
    libtrace_linktype_t     linktype)
{
    switch (linktype) {
#ifdef DLT_EN10MB
        case TRACE_TYPE_ETH:
            ctx->datalink = DLT_EN10MB;
            break;
#endif
#ifdef DLT_LINUX_SLL
        case TRACE_TYPE_LINUX_SLL:
            ctx->datalink = DLT_LINUX_SLL;
            break;
#endif
#ifdef DLT_RAW
        case TRACE_TYPE_NONE:
            ctx->datalink = DLT_RAW;
            break;
#endif
        default:
            g_warning("unknown libtrace linktype %u", linktype);
            ctx->datalink = DLT_USER15;
            break;
    }
}
#endif

/**
 * yfDecodeL2Loop
 *
 * Decode loopback packet family
 *
 */
static const uint8_t *yfDecodeL2Loop(
    yfDecodeCtx_t   *ctx,
    uint32_t        pf,
    const uint8_t   *pkt,
    uint16_t        *type)
{
    if (pf == PF_INET) {
        *type = YF_TYPE_IPv4;
    } else if ((pf == PF_INET6) || (pf == 24) || (pf == 28) ||
               (pf == 30) || (pf == 10) || (pf == 23))
    {
        /* 24 is NetBSD, OpenBSD, BSD/OS */
        /* 28 is FreeBSD, DragonFlyBSD */
        /* 30 is MacOSX */
        /* 10 is Linux */
        /* 23 is Windows (Winsock2.h)*/
        *type = YF_TYPE_IPv6;
    } else {
        ++ctx->stats.fail_l2loop;
        return NULL;
    }

    return pkt;
}

/**
 * yfDecodeL2PPP
 *
 * decode PPP header
 *
 */
static const uint8_t *yfDecodeL2PPP(
    yfDecodeCtx_t   *ctx,
    size_t          *caplen,
    const uint8_t   *pkt,
    uint16_t        *type)
{
    uint16_t        ppptype;

    /* Check for PPP header  */
    if (*caplen < 2) {
        ++ctx->stats.fail_l2hdr;
        return NULL;
    }
    /* Decode PPP type to ethertype */
    ppptype = g_ntohs(*((uint16_t *)pkt));
    switch(ppptype) {
      case YF_PPPTYPE_IPv4:
        *type = YF_TYPE_IPv4;
        break;
      case YF_PPPTYPE_IPv6:
        *type = YF_TYPE_IPv6;
        break;
      case YF_PPPTYPE_MPLS:
        *type = YF_TYPE_MPLS;
        break;
      case YF_PPPTYPE_MPLSM:
        *type = YF_TYPE_MPLSM;
        break;
      default:
        return NULL;
    }
    /* Advance packet pointer */
    pkt += 2;
    *caplen -= 2;
    return pkt;
}

/**
 * yfDecodeL2Shim
 *
 * Decode and remove supported Layer 2 shim headers (802.1q, MPLS)
 *
 *
 */
static const uint8_t *yfDecodeL2Shim(
    yfDecodeCtx_t   *ctx,
    size_t          *caplen,
    const uint8_t   *pkt,
    uint16_t        *type,
    yfL2Info_t      *l2info)
{
    uint32_t        mpls_entry;

    while (1) {
        switch (*type) {
        case YF_TYPE_8021Q:
            /* Check for full 802.1q shim header */
            if (*caplen < 4) {
                ++ctx->stats.fail_l2shim;
                return NULL;
            }
            /* Get type from 802.1q shim */
            *type = g_ntohs(((yfHdr1qShim_t *)pkt)->type);
            /* Copy out vlan tag if necessary */
            if (l2info) {
                l2info->vlan_tag =  YF_VLAN_TAG(pkt);
            }
            /* Advance packet pointer */
            *caplen -= 4;
            pkt += 4;
            /* And keep going. */
            break;
        case YF_TYPE_MPLS:
        case YF_TYPE_MPLSM:
            /* Check for full MPLS label */
            if (*caplen < 4) {
                ++ctx->stats.fail_l2shim;
                return NULL;
            }
            /* Get label entry */
            mpls_entry = g_ntohl(*((uint32_t *)(pkt)));
            /* Copy out label if necessary */
            if (l2info && l2info->mpls_count < YF_MPLS_LABEL_COUNT_MAX) {
                l2info->mpls_label[l2info->mpls_count++] =
                    YF_MPLS_LABEL(mpls_entry);
            }
            /* Advance packet pointer */
            *caplen -= 4;
            pkt += 4;
            /* Check for end of label stack */
            if (YF_MPLS_LAST(mpls_entry)) {
                YF_IP_VERSION_TO_TYPE(pkt, *caplen, *type);
            }
            break;
        case YF_TYPE_PPPOE:
            /* Check for full PPPoE header */
            if (*caplen < 6) {
                ++ctx->stats.fail_l2shim;
                return NULL;
            }
            /* We don't actually _need_ anything out of the PPPoE header.
               Just skip it. */
            *caplen -= 6;
            pkt += 6;
            /* now decode ppp */
            pkt = yfDecodeL2PPP(ctx, caplen, pkt, type);
            if (!pkt) {
                return NULL;
            }
            break;
        default:
            /* No more shim headers; type contains real ethertype. Done. */
            return pkt;
        }
    }
}

/**
 * yfDecodeL2
 *
 * Decode and remove supported Layer 2 headers
 *
 *
 */
static const uint8_t *yfDecodeL2(
    yfDecodeCtx_t   *ctx,
    size_t          *caplen,
    const uint8_t   *pkt,
    uint16_t        *type,
    yfL2Info_t      *l2info)
{
    uint32_t        pf;

    if (l2info) {
        memset(l2info, 0, sizeof(*l2info));
    }

    switch (ctx->datalink) {
#ifdef DLT_EN10MB
      case DLT_EN10MB:
#endif
#ifdef DLT_PPP_ETHER
      case DLT_PPP_ETHER:
#endif
#if defined(DLT_EN10MB) || defined(DLT_PPP_ETHER)
        /* Check for full ethernet header */
        if (*caplen < 14) {
            ++ctx->stats.fail_l2hdr;
            return NULL;
        }
        /* Copy out ethertype */
        *type = g_ntohs(((yfHdrEn10Mb_t *)pkt)->type);
        /* Copy out MAC addresses if we care */
        if (l2info) {
            memcpy(l2info->smac, ((yfHdrEn10Mb_t *)pkt)->smac, 6);
            memcpy(l2info->dmac, ((yfHdrEn10Mb_t *)pkt)->dmac, 6);
        }
        /* Advance packet pointer */
        pkt += 14;
        *caplen -= 14;
        /* Decode shim headers */
        return yfDecodeL2Shim(ctx, caplen, pkt, type, l2info);
#endif
#ifdef DLT_C_HDLC
      case DLT_C_HDLC:
        /* Check for full C-HDLC header */
        if (*caplen < 4) {
            ++ctx->stats.fail_l2hdr;
            return NULL;
        }
        /* Copy out ethertype */
        *type = g_ntohs(((yfHdrChdlc_t *)pkt)->type);
        /* Advance packet pointer */
        pkt += 4;
        *caplen -= 4;
        /* Decode shim headers */
        return yfDecodeL2Shim(ctx, caplen, pkt, type, l2info);
#endif
#ifdef DLT_LINUX_SLL
      case DLT_LINUX_SLL:
        /* Check for full Linux SLL pseudoheader */
        if (*caplen < 16) {
            ++ctx->stats.fail_l2hdr;
            return NULL;
        }
        /* Copy out ethertype */
        *type = g_ntohs(((yfHdrLinuxSll_t *)pkt)->type);
        /* Advance packet pointer */
        pkt += 16;
        *caplen -= 16;
        /* Decode shim headers */
        return yfDecodeL2Shim(ctx, caplen, pkt, type, l2info);
#endif
#ifdef DLT_PPP
      case DLT_PPP:
        /* Check for HDLC framing */
        if (*caplen < 2) {
            ++ctx->stats.fail_l2hdr;
            return NULL;
        }
        if ((pkt[0] == 0xff) && (pkt[1] == 0x03)) {
            /* Yep. HDLC framing. Strip it. */
            pkt += 2;
            *caplen -= 2;
        }
        pkt = yfDecodeL2PPP(ctx, caplen, pkt, type);
        return pkt ? yfDecodeL2Shim(ctx, caplen, pkt, type, l2info) : NULL;
#endif
#ifdef DLT_RAW
      case DLT_RAW:
        YF_IP_VERSION_TO_TYPE(pkt, *caplen, *type);
        return pkt;
#endif
#ifdef DLT_NULL
      case DLT_NULL:
        /* Check for full NULL header */
        if (*caplen < 4) {
            ++ctx->stats.fail_l2hdr;
            return NULL;
        }
        /* Grab packet family */
        pf = *(uint32_t *)pkt;
        /* Advance packet pointer */
        pkt += 4;
        *caplen -= 4;
        /* Decode loopback from packet family */
        return yfDecodeL2Loop(ctx, pf, pkt, type);
#endif
#ifdef DLT_LOOP
      case DLT_LOOP:
        /* Check for full LOOP header */
        if (*caplen < 4) {
            ++ctx->stats.fail_l2hdr;
            return NULL;
        }
        /* Grab packet family */
        pf = g_ntohl((*(uint32_t *)pkt));
        /* Advance packet pointer */
        pkt += 4;
        *caplen -= 4;
        /* Decode loopback from packet family */
        return yfDecodeL2Loop(ctx, pf, pkt, type);
#endif
#if YAF_ENABLE_LIBTRACE
      /* uninitialized libtrace */
      case DLT_USER15:
        g_warning("unknown or unset libtrace linktype");
        return NULL;
#endif
      default:
        g_warning("unknown datalink %u", ctx->datalink);
        return NULL;
    }
}

/**
 * yfDecodeIPv4
 *
 *
 *
 */
static const uint8_t *yfDecodeIPv4(
    yfDecodeCtx_t           *ctx,
    size_t                  *caplen,
    const uint8_t           *pkt,
    yfFlowKey_t             *key,
    uint16_t                *iplen,
    yfIPInfo_t              *ipinfo,
    yfIPFragInfo_t          *fraginfo)
{
    const yfHdrIPv4_t       *iph = (const yfHdrIPv4_t *)pkt;
    size_t                  iph_len;

    /* Verify we have a full IP header */
    if (*caplen < 1) {
        ++ctx->stats.fail_ip4hdr;
        return NULL;
    }

    iph_len = iph->ip_hl * 4;
    if (*caplen < iph_len) {
        ++ctx->stats.fail_ip4hdr;
        return NULL;
    }

    /* Decode source and destination address into key */
    key->version = 4;
    key->addr.v4.sip = g_ntohl(iph->ip_src);
    key->addr.v4.dip = g_ntohl(iph->ip_dst);

    /* Decode protocol into key */
    key->proto = iph->ip_p;
    /* Get IP length */
    *iplen = g_ntohs(iph->ip_len);

    /* Cap capture length to datagram length */
    if (*caplen > *iplen) {
        *caplen = *iplen;
    }

    /* Decode fragmentation information */
    if (fraginfo) {
        fraginfo->offset = g_ntohs(iph->ip_off);
        if (fraginfo->offset & (YF_IP4_OFFMASK | YF_IP4_MF)) {
            /* Packet is fragmented */
            fraginfo->frag = 1;
            /* Get ID and offset */
            fraginfo->ipid = g_ntohs(iph->ip_id);
            fraginfo->more = (fraginfo->offset & YF_IP4_MF) ? 1 : 0;
            fraginfo->offset = (fraginfo->offset & YF_IP4_OFFMASK) * 8;
            /* Stash IP header length for fragment length calculation */
            fraginfo->iphlen = iph_len;
            /* Initialize layer 4 header length */
            fraginfo->l4hlen = 0;
        } else {
            /* Packet not fragmented */
            fraginfo->frag = 0;
        }
    } else {
       /* Null fraginfo means we don't want fragments. Drop fragged packets. */
        if (g_ntohs(iph->ip_off) & (YF_IP4_OFFMASK | YF_IP4_MF)) {
            ++ctx->stats.fail_ip4frag;
            return NULL;
        }
    }

    /* Decode TTL and ECN */
    ipinfo->ttl = iph->ip_ttl;
    ipinfo->ecn = (iph->ip_tos & 0xC0) >> 6;
    
    /* Advance packet pointer */
    *caplen -= iph_len;
    return pkt + iph_len;
}


/**
 * yfDecodeIPv6
 *
 *
 *
 */
static const uint8_t *yfDecodeIPv6(
    yfDecodeCtx_t           *ctx,
    size_t                  *caplen,
    const uint8_t           *pkt,
    yfFlowKey_t             *key,
    uint16_t                *iplen,
    yfIPInfo_t              *ipinfo,
    yfIPFragInfo_t          *fraginfo)
{
    const yfHdrIPv6_t       *iph = (const yfHdrIPv6_t *)pkt;
    const yfHdrIPv6Ext_t    *ipe;
    const yfHdrIPv6Frag_t   *ipf;
    uint16_t                iph_len = 0;    /* total IP header accumulator */
    size_t                  hdr_len = 40;   /* next header length */
    uint8_t                 hdr_next;


    /* Verify that we have a full IPv6 header */
    if (*caplen < hdr_len) {
        ++ctx->stats.fail_ip6hdr;
        return NULL;
    }

    /* Decode source and destination address into key */
    memcpy(key->addr.v6.sip, &(iph->ip6_src), 16);
    memcpy(key->addr.v6.dip, &(iph->ip6_dst), 16);
    key->version = 6;

    /* Get IP length */
    *iplen = g_ntohs(iph->ip6_plen) + hdr_len;

    /* Cap capture length to datagram length */
    if (*caplen > *iplen) {
        *caplen = *iplen;
    }

    /* stash TTL and ECN */
    ipinfo->ttl = iph->ip6_hlim;
    ipinfo->ecn = YF_VCF6_ECN(iph);
    
    /* Decode next header */
    hdr_next = iph->ip6_nxt;

    /* Zero fragment flag */
    if (fraginfo) {
        fraginfo->frag = 0;
    }
    /* Now unwrap extension headers */
    while (1) {

        /* Advance packet pointer */
        *caplen -= hdr_len;
        pkt += hdr_len;
        iph_len += hdr_len;

        /* Process next extension header */
        switch (hdr_next) {
        case YF_PROTO_IP6_NONEXT:
            return NULL;
        case YF_PROTO_IP6_FRAG:
            /* Verify we have a full fragment header */
            hdr_len = 8;
            if (*caplen < hdr_len) {
                ++ctx->stats.fail_ip6ext;
                return NULL;
            }

            /* Decode fragment header */
            ipf = (const yfHdrIPv6Frag_t *)pkt;
            hdr_next = ipf->ip6f_nxt;
            if (fraginfo) {
                fraginfo->frag = 1;
                fraginfo->ipid = g_ntohl(ipf->ip6f_ident);
                fraginfo->offset = g_ntohs(ipf->ip6f_offlg);
                fraginfo->more = (fraginfo->offset | YF_IP6_MF) ? 1 : 0;
                fraginfo->offset = fraginfo->offset & YF_IP6_OFFMASK;
            } else {
            /* Null fraginfo means we don't want fragments. */
                if (g_ntohs(ipf->ip6f_offlg) & (YF_IP4_OFFMASK | YF_IP4_MF)) {
                    ++ctx->stats.fail_ip6frag;
                    return NULL;
                }
            }
            break;
        case YF_PROTO_IP6_HOP:
        case YF_PROTO_IP6_ROUTE:
        case YF_PROTO_IP6_DOPT:
            /* Verify we have the first two bytes of the extension header */
            if (*caplen < 2) {
                ++ctx->stats.fail_ip6ext;
                return NULL;
            }

            /* Get next header info */
            ipe = (const yfHdrIPv6Ext_t *)pkt;
            hdr_next = ipe->ip6e_nxt;
            hdr_len = ipe->ip6e_len * 8 + 8;
            /* Verify we have the full extension header */
            if (*caplen < hdr_len) {
                ++ctx->stats.fail_ip6ext;
                return NULL;
            }
            break;
        default:
            /* This is not an extension header. We're at layer 4 now. */
            key->proto = hdr_next;
            /* Stash total IPv6 header length for fragment length calculation */
            if (fraginfo && fraginfo->frag) {
                fraginfo->iphlen = iph_len;
                fraginfo->l4hlen = 0;
            }

            return pkt;

        }
    }
}

/**
 * yfDecodeTCP
 *
 *
 *
 */
static const uint8_t *yfDecodeTCP(
    yfDecodeCtx_t           *ctx,
    size_t                  *caplen,
    const uint8_t           *pkt,
    yfFlowKey_t             *key,
    yfIPFragInfo_t          *fraginfo,
    yfTCPInfo_t             *tcpinfo)
{
    const yfHdrTcp_t        *tcph = (const yfHdrTcp_t *)pkt;
    uint8_t to_kind, to_len;
    const yfHdrTcpOptTs_t   *tsopt;
    ssize_t                 tcph_len;

    /* Verify we have a full TCP header without options */
    if (*caplen < YF_TCP_HLEN) {
        if (fraginfo && fraginfo->frag) {
            /* will have to do TCP stuff later */
            return pkt;
        }
        ++ctx->stats.fail_l4hdr;
        return NULL;
    }

    /* get full header length */
    tcph_len = tcph->th_off * 4;
    
    /* Decode source and destination port into key */
    key->sp = g_ntohs(tcph->th_sport);
    key->dp = g_ntohs(tcph->th_dport);

    /* Copy header info */
    if (tcpinfo) {
        tcpinfo->seq = g_ntohl(tcph->th_seq);
        tcpinfo->ack = g_ntohl(tcph->th_ack);
        tcpinfo->rwin = g_ntohs(tcph->th_win);
        tcpinfo->flags = tcph->th_flags;
    }

    if (fraginfo && fraginfo->frag) {
        fraginfo->l4hlen = tcph_len;
    }
    
    /* Now verify we have all options as well */
    if (*caplen < tcph_len) {
        if (fraginfo && fraginfo->frag) {
            /* will do TCP stuff later */
            return pkt;
        }
        ++ctx->stats.fail_l4hdr;
        return NULL;
    }
    
    /* Set caplen post-options (use tcp_hlen from here for pkt bounds) */
    *caplen -= tcph_len;

    /* Skip to start of options */
    tcph_len -= YF_TCP_HLEN;
    pkt += YF_TCP_HLEN;
    
    /* Parse options while we still have them */
    while (tcpinfo && (tcph_len > 0)) {

        to_kind = *(pkt);
        
        /* handle single-byte options */
        if (to_kind == YF_TOK_EOL) goto OPT_EOL;
        if (to_kind == YF_TOK_NOP) { to_len = 1; goto OPT_NEXT;}
        
        /* get option length */
        if (tcph_len < 2) goto OPT_ERR;
        to_len = *(pkt + 1);
        
        /* die on bad option length */
        if (to_len < 2 || tcph_len < to_len) goto OPT_ERR;

        /* handle multi-byte options */
        switch (to_kind) {
            case YF_TOK_MSS:
                if (to_len < 4 || tcph_len < to_len) goto OPT_ERR;
                tcpinfo->mss = g_ntohs(*(const uint16_t *)(pkt + 2));
                break;
            case YF_TOK_WS:
                if (to_len < 3 && tcph_len < to_len) goto OPT_ERR;
                tcpinfo->ws = *(pkt + 2);
                break;
            case YF_TOK_TS:
                if (to_len < 10 || tcph_len < to_len) goto OPT_ERR;
                tsopt = (const yfHdrTcpOptTs_t*)(pkt + 2);
                tcpinfo->tsval = g_ntohl(tsopt->ts_val);
                tcpinfo->tsecr = g_ntohl(tsopt->ts_ecr);
                break;
        }
        
        /* go to next option */
OPT_NEXT:
        tcph_len -= to_len;
        pkt += to_len;
    }
    
OPT_EOL:
    /* Advance beyond any skipped options */
    return pkt + tcph_len;
    
OPT_ERR:
    ++ctx->stats.fail_l4hdr;
    return NULL;
}

/**
 * yfDefragTCP
 *
 * FIXME figure out how to reintegrate this into yfDecodeTCP.
 */
gboolean yfDefragTCP(
    uint8_t             *pkt,
    size_t              *caplen,
    yfFlowKey_t         *key,
    yfIPFragInfo_t      *fraginfo,
    yfTCPInfo_t         *tcpinfo,
    size_t              *payoff)
{

    const yfHdrTcp_t    *tcph = (const yfHdrTcp_t *)pkt;
    size_t              tcph_len;

    /* Verify we have a full TCP header */
    if (*caplen < 20) {
        return FALSE;
    }

    tcph_len = tcph->th_off * 4;
    if (*caplen < tcph_len) {
        return FALSE;
    }

    /* Decode source and destination port into key */
    key->sp = g_ntohs(tcph->th_sport);
    key->dp = g_ntohs(tcph->th_dport);

    /* Copy seq, ack, and flags */
    if (tcpinfo) {
        tcpinfo->seq = g_ntohl(tcph->th_seq);
        tcpinfo->ack = g_ntohl(tcph->th_ack);
        tcpinfo->flags = tcph->th_flags;
    }

    /* Advance packet pointer */
    *payoff += tcph_len;
    fraginfo->l4hlen = tcph_len;

    return TRUE;
}

/**
 * yfDecodeUDP
 *
 *
 *
 */
static const uint8_t *yfDecodeUDP(
    yfDecodeCtx_t           *ctx,
    size_t                  *caplen,
    const uint8_t           *pkt,
    yfFlowKey_t             *key,
    yfIPFragInfo_t          *fraginfo)
{
    const yfHdrUdp_t        *udph = (const yfHdrUdp_t *)pkt;
    const size_t            udph_len = 8;

    /* Verify we have a full UDP header */
    if (*caplen < udph_len) {
        ++ctx->stats.fail_l4hdr;
        if (fraginfo && fraginfo->frag) {
            ++ctx->stats.fail_l4frag;
        }
        return NULL;
    }

    /* Decode source and destination port into key */
    key->sp = g_ntohs(udph->uh_sport);
    key->dp = g_ntohs(udph->uh_dport);

    /* Copy header length if we're the first fragment */
    if (fraginfo && fraginfo->frag) {
        fraginfo->l4hlen = udph_len;
    }

    /* Advance packet pointer */
    *caplen -= udph_len;
    return pkt + udph_len;
}

/**
 * yfDecodeICMP
 *
 *
 *
 */
static const uint8_t *yfDecodeICMP(
    yfDecodeCtx_t           *ctx,
    size_t                  *caplen,
    const uint8_t           *pkt,
    yfFlowKey_t             *key,
    yfIPFragInfo_t          *fraginfo)
{
    const yfHdrIcmp_t       *icmph = (const yfHdrIcmp_t *)pkt;
    const size_t            icmph_len = 8;


    /* Verify we have a full ICMP header */
    if (*caplen < icmph_len) {
        ++ctx->stats.fail_l4hdr;
        if (fraginfo && fraginfo->frag) {
            ++ctx->stats.fail_l4frag;
        }
        return NULL;
    }

    /* Decode source and destination port into key */
    key->sp = 0;
    key->dp = (icmph->icmp_type << 8) + icmph->icmp_code;

    /* Copy header length if we're the first fragment */
    if (fraginfo && fraginfo->frag) {
        fraginfo->l4hlen = icmph_len;
    }

    /* Advance packet pointer */
    *caplen -= icmph_len;
    return pkt + icmph_len;
}

/* prototype needed for GRE recursion */
static const uint8_t *yfDecodeIP(
    yfDecodeCtx_t           *ctx,
    uint16_t                type,
    size_t                  *caplen,
    const uint8_t           *pkt,
    yfFlowKey_t             *key,
    uint16_t                *iplen,
    yfIPInfo_t              *ipinfo,
    yfTCPInfo_t             *tcpinfo,
    yfIPFragInfo_t          *fraginfo);


/**
 * yfDecodeGRE
 *
 *
 *
 */
static const uint8_t *yfDecodeGRE(
    yfDecodeCtx_t           *ctx,
    size_t                  *caplen,
    const uint8_t           *pkt,
    yfFlowKey_t             *key,
    uint16_t                *iplen,
    yfIPInfo_t              *ipinfo,
    yfTCPInfo_t             *tcpinfo,
    yfIPFragInfo_t          *fraginfo)
{
    const yfHdrGre_t        *greh = (const yfHdrGre_t *)pkt;
    size_t                  greh_len = 4;
    const yfHdrSre_t        *sreh = NULL;
    size_t                  sre_len = 0;

    /* Verify we have a full GRE "mandatory" header */
    /* An IP Frag has to have at least 8 - so we should never
       enter this IF */
    if (*caplen < greh_len) {
        ++ctx->stats.fail_l4hdr;
        if (fraginfo && fraginfo->frag) {
            ++ctx->stats.fail_l4frag;
        }
        return NULL;
    }

    /* Verify GRE version is 0 */
    if (YF_GHFV_VERSION(greh) != 0) {
        ++ctx->stats.fail_grevers;
        return NULL;
    }

    /* Decode the GRE header. */
    if (greh->gh_f_sum || greh->gh_f_route) {
        /* If this bit is set then the header has to contain 4 more bytes */
        /* Skip checksum and route offset */
        greh_len += 4;
    }

    if (greh->gh_f_key) {
        /* Skip key - if present header contains optional key field*/
        greh_len += 4;
    }

    if (greh->gh_f_seq) {
        /* Skip sequence number - if present header contains opt seq #*/
        greh_len += 4;
    }

    /* Verify we have a full GRE header as extended */
    if (*caplen < greh_len) {
        ++ctx->stats.fail_l4hdr;
        if (fraginfo && fraginfo->frag) {
            ++ctx->stats.fail_l4frag;
        }
        return NULL;
    }

    /* Okay. Now skip the GRE header. */
    pkt += greh_len;
    *caplen -= greh_len;

    /* Parse any SREs if present */
    if (greh->gh_f_route) {
        while (1) {
            sreh = (const yfHdrSre_t *)pkt;
            sre_len = 4;

            /* Verify we have the SRE header */
            if (*caplen < sre_len) {
                ++ctx->stats.fail_l4hdr;
                if (fraginfo && fraginfo->frag) {
                    ++ctx->stats.fail_l4frag;
                }
                return NULL;
            }

            /* Check for termination */
            if ((sreh->gh_sre_len == 0) && (g_ntohs(sreh->gh_sre_af) == 0)) {
                pkt += sre_len;
                *caplen -= sre_len;
                break;
            }

            /* Get SRE length */
            sre_len += sreh->gh_sre_len;

            /* Verify we have the full SRE*/
            if (*caplen < sre_len) {
                ++ctx->stats.fail_l4hdr;
                if (fraginfo && fraginfo->frag) {
                    ++ctx->stats.fail_l4frag;
                }
                return NULL;
            }

            /* Skip the SRE */
            pkt += sre_len;
            *caplen -= sre_len;
        }
    }

    /* We are now at the next layer header.Try to decode it as an IP header.*/
    return yfDecodeIP(ctx, g_ntohs(greh->gh_type), caplen, pkt,
                      key, iplen, ipinfo, tcpinfo, fraginfo);
}

/**
 * yfDecodeIP
 *
 *
 *
 */
static const uint8_t *yfDecodeIP(
    yfDecodeCtx_t           *ctx,
    uint16_t                type,
    size_t                  *caplen,
    const uint8_t           *pkt,
    yfFlowKey_t             *key,
    uint16_t                *iplen,
    yfIPInfo_t              *ipinfo,
    yfTCPInfo_t             *tcpinfo,
    yfIPFragInfo_t          *fraginfo)
{

    /* Check for required IP packet type. */
    if (ctx->reqtype && ctx->reqtype != type) {
        ++ctx->stats.fail_l3type;
        return NULL;
    }

    /* Unwrap and decode IP headers */
    switch (type) {
     case YF_TYPE_IPv4:
        if (!(pkt = yfDecodeIPv4(ctx, caplen, pkt, key,
                                 iplen, ipinfo, fraginfo)))
        {
            return NULL;
        }
        break;
      case YF_TYPE_IPv6:
         if (!(pkt = yfDecodeIPv6(ctx, caplen, pkt, key,
                                  iplen, ipinfo, fraginfo)))
         {
            return NULL;
        }
        break;
      case YF_TYPE_ARP:
        ++ctx->stats.fail_arptype;
        return NULL;
      default:
        ++ctx->stats.fail_l3type;
        return NULL;
    }

    /* Skip layer 4 decode unless we're the first fragment */
    if (fraginfo && fraginfo->frag && fraginfo->offset) {
        return pkt;
    }

    /* Unwrap and decode layer 4 headers */
    switch (key->proto) {
      case YF_PROTO_TCP:
        if (!(pkt = yfDecodeTCP(ctx, caplen, pkt, key, fraginfo, tcpinfo))) {
            return NULL;
        }
        break;
      case YF_PROTO_UDP:
        if (!(pkt = yfDecodeUDP(ctx, caplen, pkt, key, fraginfo))) {
            return NULL;
        }
        break;
      case YF_PROTO_ICMP:
      case YF_PROTO_ICMP6:
        if (!(pkt = yfDecodeICMP(ctx, caplen, pkt, key, fraginfo))) {
            return NULL;
        }
        break;
      case YF_PROTO_GRE:
        if (ctx->gremode) {
            if (!(pkt = yfDecodeGRE(ctx, caplen, pkt, key,
                                    iplen, ipinfo, tcpinfo, fraginfo))) {
                return NULL;
            }
        } else {
            /* Not decoding GRE. Zero ports. */
            key->sp = 0;
            key->dp = 0;
        }
        break;
      default:
        /* No layer 4 header we understand. Zero ports. */
        key->sp = 0;
        key->dp = 0;
    }

    /* Return what's left of the packet */
    return pkt;
}

/**
 * yfDecodeToPBuf
 *
 *
 *
 */
gboolean yfDecodeToPBuf(
    yfDecodeCtx_t           *ctx,
    uint64_t                ptime,
    size_t                  caplen,
    const uint8_t           *pkt,
    yfIPFragInfo_t          *fraginfo,
    yfPBuf_t                *pbuf)
{
    uint16_t                type;
    yfFlowKey_t             *key = &(pbuf->key);
    uint16_t                *iplen = &(pbuf->iplen);
    yfTCPInfo_t             *tcpinfo = &(pbuf->tcpinfo);
    yfIPInfo_t              *ipinfo = &(pbuf->ipinfo);
    yfL2Info_t              *l2info = &(pbuf->l2info);
    const uint8_t           *ipTcpHeaderStart = NULL;
    size_t                  capb4l2 = caplen;
    
    /* Zero packet buffer time (mark it not yet valid) */
    pbuf->ptime = 0;

    /* Keep the start of pcap for pcap output */
    ipTcpHeaderStart = pkt;

    /* Unwrap layer 2 headers */
    if (!(pkt = yfDecodeL2(ctx, &caplen, pkt, &type, l2info))) {
        return FALSE;
    }

    l2info->l2hlen = (uint16_t)(capb4l2 - caplen);
    if (l2info) {
        key->vlanId = l2info->vlan_tag;
    } else {
        key->vlanId = 0;
    }

    /* Now we should have an IP packet. Decode it. */
    if (!(pkt = yfDecodeIP(ctx, type, &caplen, pkt, key, iplen,
                           ipinfo, tcpinfo, fraginfo))) {
        return FALSE;
    }

    /* Copy ctime into packet buffer */
    pbuf->ptime = ptime;

    /* Keep track of how far we progressed */
    pbuf->allHeaderLen = pkt - ipTcpHeaderStart;

    caplen = caplen + pbuf->allHeaderLen;

    return TRUE;
}

/**
 * yfDecodeCtxAlloc
 *
 *
 *
 */
yfDecodeCtx_t *yfDecodeCtxAlloc(
    int             datalink,
    uint16_t        reqtype,
    gboolean        gremode)
{
    yfDecodeCtx_t   *ctx = NULL;

    /* Allocate a decode context */
    ctx = yg_slice_new0(yfDecodeCtx_t);

    /* Fill in the configuration */
    ctx->datalink = datalink;
    ctx->reqtype = reqtype;
    ctx->gremode = gremode;

    /* Done */
    return ctx;
}

/**
 * yfDecodeCtxFree
 *
 *
 *
 */
void yfDecodeCtxFree(
    yfDecodeCtx_t           *ctx)
{
    /* just free the context */
    yg_slice_free(yfDecodeCtx_t, ctx);
}


/**
 * yfDecodeTimeval
 *
 *
 *
 */
uint64_t yfDecodeTimeval(
    const struct timeval    *tv)
{
    return (((uint64_t)tv->tv_sec * 1000) + ((uint64_t)tv->tv_usec / 1000));
}

/**
 * yfDecodeTimeNTP
 *
 *
 *
 */
uint64_t yfDecodeTimeNTP(
    uint64_t                ntp)
{
    double          dntp;

    dntp = (ntp & 0xFFFFFFFF00000000LL) >> 32;
    dntp += ((ntp & 0x00000000FFFFFFFFLL) * 1.0) / (2LL << 32);

    dntp *= 1000;
    return (uint64_t)dntp;
}

/**
 * yfGetDecodeStats
 *
 */
uint32_t yfGetDecodeStats(
    yfDecodeCtx_t *ctx)
{
    uint32_t            fail_snaptotal;
    uint32_t            fail_suptotal;
    uint32_t            fail_total;

    fail_snaptotal =
        ctx->stats.fail_l2hdr + ctx->stats.fail_l2shim +
        ctx->stats.fail_ip4hdr + ctx->stats.fail_ip6hdr +
        ctx->stats.fail_ip6ext + ctx->stats.fail_l4hdr;

    fail_suptotal =
        ctx->stats.fail_l2loop + ctx->stats.fail_l3type +
        ctx->stats.fail_ip4frag + ctx->stats.fail_ip6frag +
        ctx->stats.fail_grevers + ctx->stats.fail_arptype;

    fail_total =
        fail_snaptotal + fail_suptotal;

    return fail_total;
}

/**
 * yfDecodeDumpStats
 *
 *
 *
 */
void yfDecodeDumpStats(
    yfDecodeCtx_t       *ctx,
    uint64_t            packetTotal)
{
    uint32_t            fail_snaptotal;
    uint32_t            fail_suptotal;
    uint32_t            fail_total;

    fail_snaptotal =
        ctx->stats.fail_l2hdr + ctx->stats.fail_l2shim +
        ctx->stats.fail_ip4hdr + ctx->stats.fail_ip6hdr +
        ctx->stats.fail_ip6ext + ctx->stats.fail_l4hdr;

    fail_suptotal =
        ctx->stats.fail_l2loop + ctx->stats.fail_l3type +
        ctx->stats.fail_ip4frag + ctx->stats.fail_ip6frag +
        ctx->stats.fail_grevers + ctx->stats.fail_arptype;

    fail_total =
        fail_snaptotal + fail_suptotal;

    /* fail_total isn't counted in packetTotal(flowtab) - so add
       packetTotal & fail_total to get TOTAL packets processed by YAF. */
    packetTotal += fail_total;

    if (fail_total) {
        g_debug("Rejected %u packets during decode: (%3.2f%%)",
                fail_total,
                ((double)(fail_total)/(double)(packetTotal) * 100) );

        if (fail_snaptotal) {
            g_debug("  %u due to incomplete headers: (%3.2f%%)",
                fail_snaptotal,
                ((double)(fail_snaptotal)/(double)(packetTotal) * 100) );
            if (ctx->stats.fail_l2hdr) {
                g_debug("    %u incomplete layer 2 headers. (%3.2f%%)",
                    ctx->stats.fail_l2hdr,
                    ((double)(ctx->stats.fail_l2hdr)/(double)(packetTotal) * 100) );
            }
            if (ctx->stats.fail_l2shim) {
                g_debug("    %u incomplete shim headers. (%3.2f%%)",
                    ctx->stats.fail_l2shim,
                        ((double)(ctx->stats.fail_l2shim)/(double)(packetTotal) * 100) );
            }
            if (ctx->stats.fail_ip4hdr) {
                g_debug("    %u incomplete IPv4 headers. (%3.2f%%)",
                        ctx->stats.fail_ip4hdr,
                        ((double)(ctx->stats.fail_ip4hdr)/(double)(packetTotal) * 100) );
            }

            if (ctx->stats.fail_ip6hdr) {
                g_debug("    %u incomplete IPv6 headers. (%3.2f%%)",
                        ctx->stats.fail_ip6hdr,
                        ((double)(ctx->stats.fail_ip6hdr)/(double)(packetTotal) * 100) );
            }

            if (ctx->stats.fail_ip6ext) {
                g_debug("    %u incomplete IPv6 extension headers. (%3.2f%%)",
                        ctx->stats.fail_ip6ext,
                        ((double)(ctx->stats.fail_ip6ext)/(double)(packetTotal) * 100) );
            }
            if (ctx->stats.fail_l4hdr) {
                g_debug("    %u incomplete transport headers. (%3.2f%%)",
                        ctx->stats.fail_l4hdr,
                        ((double)(ctx->stats.fail_l4hdr)/(double)(packetTotal) * 100) );
                if (ctx->stats.fail_l4frag) {
                    g_debug("      (%u fragmented.) (%3.2f%%)",
                        ctx->stats.fail_l4frag,
                        ((double)(ctx->stats.fail_l4frag)/(double)(packetTotal) * 100) );
                }
            }
            g_debug("    (Use a larger snaplen to reduce incomplete headers.)");
        }

        if (fail_suptotal) {
            g_debug("  %u due to unsupported/rejected packet type: (%3.2f%%)",
                fail_suptotal,
                ((double)(fail_suptotal)/(double)(packetTotal) * 100) );
            if (ctx->stats.fail_l3type) {
                g_debug("    %u unsupported/rejected Layer 3 headers. (%3.2f%%)",
                    ctx->stats.fail_l3type + ctx->stats.fail_arptype,
                    ((double)(ctx->stats.fail_l3type + ctx->stats.fail_arptype)/
                     (double)(packetTotal) * 100) );
            }
            if (ctx->stats.fail_arptype) {
                g_debug("    %u ARP packets. (%3.2f%%)", ctx->stats.fail_arptype,
                        ((double)(ctx->stats.fail_arptype)/(double)(packetTotal) * 100));
            }
            if (ctx->stats.fail_ip4frag) {
                g_debug("    %u IPv4 fragments. (%3.2f%%)",
                    ctx->stats.fail_ip4frag,
                    ((double)(ctx->stats.fail_ip4frag)/(double)(packetTotal) * 100) );
            }
            if (ctx->stats.fail_ip6frag) {
                g_debug("    %u IPv6 fragments. (%3.2f%%)",
                    ctx->stats.fail_ip6frag,
                    ((double)(ctx->stats.fail_ip6frag)/(double)(packetTotal) * 100) );
            }
            if (ctx->stats.fail_l2loop) {
                g_debug("    %u unsupported loopback packet families. (%3.2f%%)",
                    ctx->stats.fail_l2loop,
                    ((double)(ctx->stats.fail_l2loop)/(double)(packetTotal) * 100) );
            }
            if (ctx->stats.fail_grevers) {
                g_debug("    %u unsupported GRE version headers. (%3.2f%%)",
                        ctx->stats.fail_grevers,
                        ((double)(ctx->stats.fail_grevers)/(double)(packetTotal) * 100) );
            }
        }
    }
}
