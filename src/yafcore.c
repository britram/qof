/**
 ** @internal
 ** yafcore.c
 ** YAF core I/O routines
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2006-2012 Carnegie Mellon University. All Rights Reserved.
 ** Copyright (C) 2012-2013 Brian Trammell.             All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell, Chris Inacio, Emily Ecoff <ecoff@cert.org>
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
#include "qofconfig.h"
#include "yafstat.h"

#include <yaf/yafcore.h>
#include <yaf/decode.h>
#include <airframe/airutil.h>
#include <yaf/yafrag.h>

/** These are the template IDs for the templates that YAF uses to
    select the output. Template IDs are maintained for a set of
    basic flow types data
    * BASE which gets various additions added as the flow requires,
    * FULL base plus the internal fields are added
    * EXT (extended) which has the additional records in the
      yaf_extime_spec (extended time specification)

    WARNING: these need to be adjusted according to changes in the
    general & special dimensions */
#define YAF_FLOW_BASE_TID      0xB000 /* no general or special definitions */
#define YAF_FLOW_FULL_TID      0xB800 /* base no internal */
#define YAF_FLOW_EXT_TID       0xB7FF /* everything except internal */
                               
#define YAF_OPTIONS_TID        0xD000

/* 49154 - 49160 */
#define YAF_STATS_FLOW_TID     0xC005

/** The dimensions are flags which determine which sets of fields will
    be exported out to an IPFIX record.  They are entries in a bitmap
    used to control the template. e.g. TCP flow information (seq num,
    tcp flags, etc.) only get added to the output record when the
    YTF_TCP flag is set; it only gets set when the transport protocol
    is set to 0x06. */

/** General dimensions -- these are either present or not */
#define YTF_BIF         0x0001  /* Biflow */
#define YTF_TCP         0x0002  /* TCP and extended TCP */
#define YTF_RTT         0x0004  /* valid RTT information available */
#define YTF_TSV         0x0008  /* valid timestamp information available */

/* Special dimensions -- one of each group must be present */
#define YTF_IP4         0x0010  /* IPv4 addresses */
#define YTF_IP6         0x0020  /* IPv6 addresses */
#define YTF_FLE         0x0040  /* full-length encoding */
#define YTF_RLE         0x0080  /* reduced-length encoding */

#define YTF_INTERNAL    0x0100 /* internal (padding) IEs */
#define YTF_ALL         0x007F /* this has to be everything _except_ RLE enabled */

/** If any of the FLE/RLE values are larger than this constant
    then we have to use FLE, otherwise, we choose RLE to
    conserve space/bandwidth etc.*/
#define YAF_RLEMAX      (1L << 31)

#define YF_PRINT_DELIM  "|"

#define QOF_MIN_RTT_COUNT 1

/** include enterprise-specific Information Elements for YAF */
#include "yaf/CERT_IE.h"
#include "yaf/TCH_IE.h"
#include "yaf/IANA_IE.h"

static uint64_t yaf_start_time = 0;

/* Internal flow record template. Must match structure of yfIpfixFlow_t */
static fbInfoElementSpec_t qof_internal_spec[] = {
    /* Flow ID */
    { "flowId",                             8, 0 },
    /* Timers and counters */
    { "flowStartMilliseconds",              8, 0 },
    { "flowEndMilliseconds",                8, 0 },
    { "octetDeltaCount",                    8, YTF_FLE },
    { "reverseOctetDeltaCount",             8, YTF_FLE | YTF_BIF },
    { "packetDeltaCount",                   8, YTF_FLE },
    { "reversePacketDeltaCount",            8, YTF_FLE | YTF_BIF },
    { "transportOctetDeltaCount",                    8, YTF_FLE },
    { "reverseTransportOctetDeltaCount",                    8, YTF_FLE | YTF_BIF },
    { "transportPacketDeltaCount",                   8, YTF_FLE },
    { "reverseTransportPacketDeltaCount",                   8, YTF_FLE | YTF_BIF },
    { "octetDeltaCount",                    4, YTF_RLE },
    { "reverseOctetDeltaCount",             4, YTF_RLE | YTF_BIF },
    { "packetDeltaCount",                   4, YTF_RLE },
    { "reversePacketDeltaCount",            4, YTF_RLE | YTF_BIF },
    { "transportOctetDeltaCount",                    4, YTF_RLE },
    { "reverseTransportOctetDeltaCount",                    4, YTF_RLE | YTF_BIF },
    { "transportPacketDeltaCount",                   4, YTF_RLE },
    { "reverseTransportPacketDeltaCount",                   4, YTF_RLE | YTF_BIF },
    /* Addresses */
    { "sourceIPv4Address",                  4, YTF_IP4 },
    { "destinationIPv4Address",             4, YTF_IP4 },
    { "sourceIPv6Address",                  16, YTF_IP6 },
    { "destinationIPv6Address",             16, YTF_IP6 },
    /* Extended TCP counters and performance info */
    { "tcpSequenceCount",                   8, YTF_TCP | YTF_FLE },
    { "reverseTcpSequenceCount",            8, YTF_TCP | YTF_FLE | YTF_BIF },
    { "tcpSequenceLossCount",               8, YTF_TCP | YTF_FLE },
    { "reverseTcpSequenceLossCount",        8, YTF_TCP | YTF_FLE | YTF_BIF },
    { "tcpRetransmitCount",                 8, YTF_TCP | YTF_FLE },
    { "reverseTcpRetransmitCount",          8, YTF_TCP | YTF_FLE | YTF_BIF },
    { "tcpLossEventCount",                   8, YTF_TCP | YTF_FLE },
    { "reverseTcpLossEventCount",            8, YTF_TCP | YTF_FLE | YTF_BIF },    
    { "tcpSequenceJumpCount",                 8, YTF_TCP | YTF_FLE },
    { "reverseTcpSequenceJumpCount",          8, YTF_TCP | YTF_FLE | YTF_BIF },
    { "tcpDupAckCount",                     8, YTF_TCP | YTF_FLE | YTF_BIF },
    { "reverseTcpDupAckCount",              8, YTF_TCP | YTF_FLE | YTF_BIF },
    { "tcpSelAckCount",                     8, YTF_TCP | YTF_FLE | YTF_BIF },
    { "reverseTcpSelAckCount",              8, YTF_TCP | YTF_FLE | YTF_BIF },
    { "tcpSequenceCount",                   4, YTF_TCP | YTF_RLE },
    { "reverseTcpSequenceCount",            4, YTF_TCP | YTF_RLE | YTF_BIF },
    { "tcpSequenceLossCount",               4, YTF_TCP | YTF_RLE },
    { "reverseTcpSequenceLossCount",        4, YTF_TCP | YTF_RLE | YTF_BIF },
    { "tcpRetransmitCount",                 4, YTF_TCP | YTF_RLE },
    { "reverseTcpRetransmitCount",          4, YTF_TCP | YTF_RLE | YTF_BIF },
    { "tcpLossEventCount",                   4, YTF_TCP | YTF_RLE },
    { "reverseTcpLossEventCount",            4, YTF_TCP | YTF_RLE | YTF_BIF },
    { "tcpSequenceJumpCount",                 4, YTF_TCP | YTF_RLE },
    { "reverseTcpSequenceJumpCount",          4, YTF_TCP | YTF_RLE | YTF_BIF },
    { "tcpDupAckCount",                     4, YTF_TCP | YTF_RLE | YTF_BIF },
    { "reverseTcpDupAckCount",              4, YTF_TCP | YTF_RLE | YTF_BIF },
    { "tcpSelAckCount",                     4, YTF_TCP | YTF_RLE | YTF_BIF },
    { "reverseTcpSelAckCount",              4, YTF_TCP | YTF_RLE | YTF_BIF },
    { "tcpSequenceNumber",                  4, YTF_TCP },
    { "reverseTcpSequenceNumber",           4, YTF_TCP | YTF_BIF },
    { "maxTcpSequenceJump",                 4, YTF_TCP },
    { "reverseMaxTcpSequenceJump",          4, YTF_TCP | YTF_BIF},
    { "qofTcpCharacteristics",              4, YTF_TCP },
    { "reverseQofTcpCharacteristics",       4, YTF_TCP | YTF_BIF},
    { "minTcpRwin",                         4, YTF_TCP },
    { "reverseMinTcpRwin",                  4, YTF_TCP | YTF_BIF},
    { "meanTcpRwin",                        4, YTF_TCP },
    { "reverseMeanTcpRwin",                 4, YTF_TCP | YTF_BIF},
    { "maxTcpRwin",                         4, YTF_TCP },
    { "reverseMaxTcpRwin",                  4, YTF_TCP | YTF_BIF},
    { "tcpReceiverStallCount",              4, YTF_TCP },
    { "reverseTcpReceiverStallCount",       4, YTF_TCP | YTF_BIF},
    { "tcpTimestampFrequency",              4, YTF_TCP | YTF_TSV },
    { "reverseTcpTimestampFrequency",       4, YTF_TCP | YTF_TSV | YTF_BIF},
    { "tcpRttSampleCount",                  4, YTF_RTT },
    { "lastTcpRttMilliseconds",             2, YTF_RTT },
    { "minTcpRttMilliseconds",              2, YTF_RTT },
    { "declaredTcpMss",                     2, YTF_TCP },
    { "reverseDeclaredTcpMss",              2, YTF_TCP | YTF_BIF },
    { "observedTcpMss",                     2, YTF_TCP },
    { "reverseObservedTcpMss",              2, YTF_TCP | YTF_BIF },
    { "minTcpIOTMilliseconds",              4, YTF_TCP },
    { "reverseMinTcpIOTMilliseconds",       4, YTF_TCP | YTF_BIF},
    { "maxTcpIOTMilliseconds",              4, YTF_TCP },
    { "reverseMaxTcpIOTMilliseconds",       4, YTF_TCP | YTF_BIF},
    { "minTcpChirpMilliseconds",            2, YTF_TCP | YTF_TSV},
    { "reverseMinTcpChirpMilliseconds",     2, YTF_TCP | YTF_TSV | YTF_BIF},
    { "maxTcpChirpMilliseconds",            2, YTF_TCP | YTF_TSV },
    { "reverseMaxTcpChirpMilliseconds",     2, YTF_TCP | YTF_TSV | YTF_BIF},
    /* First-packet RTT (for all biflows) */
    { "reverseFlowDeltaMilliseconds",       4, YTF_BIF },
    /* port, protocol, flow status, interfaces */
    { "sourceTransportPort",                2, 0 },
    { "destinationTransportPort",           2, 0 },
    { "protocolIdentifier",                 1, 0 },
    { "flowEndReason",                      1, 0 },
    { "ingressInterface",                   1, 0 },
    { "egressInterface",                    1, 0 },
    /* Layer 2 information */
    { "sourceMacAddress",                   6, 0 },
    { "destinationMacAddress",              6, 0 },
    { "vlanId",                             2, 0 },
    /* Layer 3 information */
    { "minimumTTL",                         1, 0},
    { "maximumTTL",                         1, 0},
    { "reverseMinimumTTL",                  1, YTF_BIF },
    { "reverseMaximumTTL",                  1, YTF_BIF },
    /* Layer 4 information */
    { "initialTCPFlags",                    1, YTF_TCP },
    { "reverseInitialTCPFlags",             1, YTF_TCP | YTF_BIF },
    { "unionTCPFlags",                      1, YTF_TCP },
    { "reverseUnionTCPFlags",               1, YTF_TCP | YTF_BIF },
    { "tcpControlBits",                     1, YTF_TCP },
    { "reverseTcpControlBits",              1, YTF_TCP | YTF_BIF },
FB_IESPEC_NULL
};

#define QOF_EXPORT_SPEC_SZ 96
static fbInfoElementSpec_t qof_export_spec[QOF_EXPORT_SPEC_SZ];
static size_t qof_export_spec_count = 0;

static fbInfoElementSpec_t yaf_stats_option_spec[] = {
    { "systemInitTimeMilliseconds",         0, 0 },
    { "exportedFlowRecordTotalCount",       0, 0 },
    { "packetTotalCount",                   0, 0 },
    { "droppedPacketTotalCount",            0, 0 },
    { "ignoredPacketTotalCount",            0, 0 },
    { "notSentPacketTotalCount",            0, 0 },
    { "expiredFragmentCount",               0, 0 },
    { "assembledFragmentCount",             0, 0 },
    { "flowTableFlushEventCount",           0, 0 },
    { "flowTablePeakCount",                 0, 0 },
    { "exporterIPv4Address",                0, 0 },
    { "exportingProcessId",                 0, 0 },
    FB_IESPEC_NULL
};
/* IPv6-mapped IPv4 address prefix */
static uint8_t yaf_ip6map_pfx[12] =
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF };

/* Full YAF flow record. */
typedef struct yfIpfixFlow_st {
    /* Flow ID */
    uint64_t    flowId;
    /* Timers and counters */
    uint64_t    flowStartMilliseconds;
    uint64_t    flowEndMilliseconds;
    uint64_t    octetCount;
    uint64_t    reverseOctetCount;
    uint64_t    packetCount;
    uint64_t    reversePacketCount;
    uint64_t    transportOctetDeltaCount;
    uint64_t    reverseTransportOctetDeltaCount;
    uint64_t    transportPacketDeltaCount;
    uint64_t    reverseTransportPacketDeltaCount;
    /* Addresses */
    uint32_t    sourceIPv4Address;
    uint32_t    destinationIPv4Address;
    uint8_t     sourceIPv6Address[16];
    uint8_t     destinationIPv6Address[16];
    /* Extended TCP counters and performance info */
    uint64_t    tcpSequenceCount;
    uint64_t    reverseTcpSequenceCount;
    uint64_t    tcpSequenceLossCount;
    uint64_t    reverseTcpSequenceLossCount;
    uint64_t    tcpRetransmitCount;
    uint64_t    reverseTcpRetransmitCount;
    uint64_t    tcpLossEventCount;
    uint64_t    reverseTcpLossEventCount;
    uint64_t    tcpSequenceJumpCount;
    uint64_t    reverseTcpSequenceJumpCount;
    uint64_t    tcpDupAckCount;
    uint64_t    reverseTcpDupAckCount;
    uint64_t    tcpSelAckCount;
    uint64_t    reverseTcpSelAckCount;
    uint32_t    tcpSequenceNumber;
    uint32_t    reverseTcpSequenceNumber;
    uint32_t    maxTcpSequenceJump;
    uint32_t    reverseMaxTcpSequenceJump;
    uint32_t    qofTcpCharacteristics;
    uint32_t    reverseQofTcpCharacteristics;
    uint32_t    minTcpRwin;
    uint32_t    reverseMinTcpRwin;
    uint32_t    meanTcpRwin;
    uint32_t    reverseMeanTcpRwin;
    uint32_t    maxTcpRwin;
    uint32_t    reverseMaxTcpRwin;
    uint32_t    tcpReceiverStallCount;
    uint32_t    reverseTcpReceiverStallCount;
    uint32_t    tcpTimestampFrequency;
    uint32_t    reverseTcpTimestampFrequency;
    uint32_t    tcpRttSampleCount;
    uint16_t    lastTcpRttMilliseconds;
    uint16_t    minTcpRttMilliseconds;
    uint16_t    declaredTcpMss;
    uint16_t    reverseDeclaredTcpMss;
    uint16_t    observedTcpMss;
    uint16_t    reverseObservedTcpMss;
    uint32_t    minTcpIOTMilliseconds;
    uint32_t    reverseMinTcpIOTMilliseconds;
    uint32_t    maxTcpIOTMilliseconds;
    uint32_t    reverseMaxTcpIOTMilliseconds;
    int16_t     minTcpChirpMilliseconds;
    int16_t     reverseMinTcpChirpMilliseconds;
    int16_t     maxTcpChirpMilliseconds;
    int16_t     reverseMaxTcpChirpMilliseconds;
    /* First-packet RTT */
    int32_t     reverseFlowDeltaMilliseconds;
    /* Flow key */
    uint16_t    sourceTransportPort;
    uint16_t    destinationTransportPort;
    uint8_t     protocolIdentifier;
    uint8_t     flowEndReason;
    uint8_t     ingressInterface;
    uint8_t     egressInterface;
    /* Layer 2 Information */
    uint8_t     sourceMacAddress[6];
    uint8_t     destinationMacAddress[6];
    uint16_t    vlanId;
    /* Layer 3 Information */
    uint8_t     minimumTTL;
    uint8_t     maximumTTL;
    uint8_t     reverseMinimumTTL;
    uint8_t     reverseMaximumTTL;
    /* Layer 4 Information */
    uint8_t     initialTCPFlags;
    uint8_t     reverseInitialTCPFlags;
    uint8_t     unionTCPFlags;
    uint8_t     reverseUnionTCPFlags;
    uint8_t     tcpControlBits;
    uint8_t     reverseTcpControlBits;
} yfIpfixFlow_t;

typedef struct yfIpfixStats_st {
    uint64_t    systemInitTimeMilliseconds;
    uint64_t    exportedFlowTotalCount;
    uint64_t    packetTotalCount;
    uint64_t    droppedPacketTotalCount;
    uint64_t    ignoredPacketTotalCount;
    uint64_t    notSentPacketTotalCount;
    uint32_t    expiredFragmentCount;
    uint32_t    assembledFragmentCount;
    uint32_t    flowTableFlushEvents;
    uint32_t    flowTablePeakCount;
    uint32_t    exporterIPv4Address;
    uint32_t    exportingProcessId;
} yfIpfixStats_t;

/* Core library configuration variables */
static gboolean yaf_core_map_ipv6 = FALSE;
static gboolean yaf_core_force_biflow = FALSE;
static qfIfMap_t *yaf_core_ifmap = NULL;
static qfNetList_t *yaf_source_netlist = NULL;
static qfMacList_t *yaf_source_maclist = NULL;

/**
 * qfInternalTemplateCheck
 *
 * this checks the alignment of the template and corresponding record
 * ideally, all this magic would happen at compile time, but it
 * doesn't currently, (can't really do it in C,) so we do it at
 * run time.
 *
 * Causes QoF to crash if alignment is incorrect
 *
 */


void qfInternalTemplateCheck() {
#define EO_STRING(S_,F_) "template offset mismatch in " #S_ " for element " #F_ \
                         "(tmpl %" SIZE_T_FORMAT " struct %" SIZE_T_FORMAT ")"
#define CHECK_OFFSET(S_,F_) \
    if (offsetof(S_,F_) != internal_offsets[k++]) \
        g_error(EO_STRING(S_,F_), (SIZE_T_CAST)internal_offsets[k-1], \
                                  (SIZE_T_CAST)offsetof(S_,F_));
    
    size_t internal_offsets[QOF_EXPORT_SPEC_SZ];
    size_t next_offset = 0;
    int i = 0, j = 0, k = 0;
    
    /* build offsets array for main template*/
    for (i = 0; qof_internal_spec[i].name; ++i) {
        if (!(qof_internal_spec[i].flags & YTF_RLE)) {
            internal_offsets[j++] = next_offset;
            next_offset += qof_internal_spec[i].len_override;
        }
    }
    
    /* check template offset match in order */
    CHECK_OFFSET(yfIpfixFlow_t,flowId);
    CHECK_OFFSET(yfIpfixFlow_t,flowStartMilliseconds);
    CHECK_OFFSET(yfIpfixFlow_t,flowEndMilliseconds);
    CHECK_OFFSET(yfIpfixFlow_t,octetCount);
    CHECK_OFFSET(yfIpfixFlow_t,reverseOctetCount);
    CHECK_OFFSET(yfIpfixFlow_t,packetCount);
    CHECK_OFFSET(yfIpfixFlow_t,reversePacketCount);
    CHECK_OFFSET(yfIpfixFlow_t,transportOctetDeltaCount);
    CHECK_OFFSET(yfIpfixFlow_t,reverseTransportOctetDeltaCount);
    CHECK_OFFSET(yfIpfixFlow_t,transportPacketDeltaCount);
    CHECK_OFFSET(yfIpfixFlow_t,reverseTransportPacketDeltaCount);
    CHECK_OFFSET(yfIpfixFlow_t,sourceIPv4Address);
    CHECK_OFFSET(yfIpfixFlow_t,destinationIPv4Address);
    CHECK_OFFSET(yfIpfixFlow_t,sourceIPv6Address);
    CHECK_OFFSET(yfIpfixFlow_t,destinationIPv6Address);
    CHECK_OFFSET(yfIpfixFlow_t,tcpSequenceCount);
    CHECK_OFFSET(yfIpfixFlow_t,reverseTcpSequenceCount);
    CHECK_OFFSET(yfIpfixFlow_t,tcpSequenceLossCount);
    CHECK_OFFSET(yfIpfixFlow_t,reverseTcpSequenceLossCount);
    CHECK_OFFSET(yfIpfixFlow_t,tcpRetransmitCount);
    CHECK_OFFSET(yfIpfixFlow_t,reverseTcpRetransmitCount);
    CHECK_OFFSET(yfIpfixFlow_t,tcpLossEventCount);
    CHECK_OFFSET(yfIpfixFlow_t,reverseTcpLossEventCount);
    CHECK_OFFSET(yfIpfixFlow_t,tcpSequenceJumpCount);
    CHECK_OFFSET(yfIpfixFlow_t,reverseTcpSequenceJumpCount);
    CHECK_OFFSET(yfIpfixFlow_t,tcpDupAckCount);
    CHECK_OFFSET(yfIpfixFlow_t,reverseTcpDupAckCount);
    CHECK_OFFSET(yfIpfixFlow_t,tcpSelAckCount);
    CHECK_OFFSET(yfIpfixFlow_t,reverseTcpSelAckCount);
    CHECK_OFFSET(yfIpfixFlow_t,tcpSequenceNumber);
    CHECK_OFFSET(yfIpfixFlow_t,reverseTcpSequenceNumber);
    CHECK_OFFSET(yfIpfixFlow_t,maxTcpSequenceJump);
    CHECK_OFFSET(yfIpfixFlow_t,reverseMaxTcpSequenceJump);
    CHECK_OFFSET(yfIpfixFlow_t,qofTcpCharacteristics);
    CHECK_OFFSET(yfIpfixFlow_t,reverseQofTcpCharacteristics);
    CHECK_OFFSET(yfIpfixFlow_t,minTcpRwin);
    CHECK_OFFSET(yfIpfixFlow_t,reverseMinTcpRwin);
    CHECK_OFFSET(yfIpfixFlow_t,meanTcpRwin);
    CHECK_OFFSET(yfIpfixFlow_t,reverseMeanTcpRwin);
    CHECK_OFFSET(yfIpfixFlow_t,maxTcpRwin);
    CHECK_OFFSET(yfIpfixFlow_t,reverseMaxTcpRwin);
    CHECK_OFFSET(yfIpfixFlow_t,tcpReceiverStallCount);
    CHECK_OFFSET(yfIpfixFlow_t,reverseTcpReceiverStallCount);
    CHECK_OFFSET(yfIpfixFlow_t,tcpTimestampFrequency);
    CHECK_OFFSET(yfIpfixFlow_t,reverseTcpTimestampFrequency);
    CHECK_OFFSET(yfIpfixFlow_t,tcpRttSampleCount);
    CHECK_OFFSET(yfIpfixFlow_t,lastTcpRttMilliseconds);
    CHECK_OFFSET(yfIpfixFlow_t,minTcpRttMilliseconds);
    CHECK_OFFSET(yfIpfixFlow_t,declaredTcpMss);
    CHECK_OFFSET(yfIpfixFlow_t,reverseDeclaredTcpMss);
    CHECK_OFFSET(yfIpfixFlow_t,observedTcpMss);
    CHECK_OFFSET(yfIpfixFlow_t,reverseObservedTcpMss);
    CHECK_OFFSET(yfIpfixFlow_t,minTcpIOTMilliseconds);
    CHECK_OFFSET(yfIpfixFlow_t,reverseMinTcpIOTMilliseconds);
    CHECK_OFFSET(yfIpfixFlow_t,maxTcpIOTMilliseconds);
    CHECK_OFFSET(yfIpfixFlow_t,reverseMaxTcpIOTMilliseconds);
    CHECK_OFFSET(yfIpfixFlow_t,minTcpChirpMilliseconds);
    CHECK_OFFSET(yfIpfixFlow_t,reverseMinTcpChirpMilliseconds);
    CHECK_OFFSET(yfIpfixFlow_t,maxTcpChirpMilliseconds);
    CHECK_OFFSET(yfIpfixFlow_t,reverseMaxTcpChirpMilliseconds);
    CHECK_OFFSET(yfIpfixFlow_t,reverseFlowDeltaMilliseconds);
    CHECK_OFFSET(yfIpfixFlow_t,sourceTransportPort);
    CHECK_OFFSET(yfIpfixFlow_t,destinationTransportPort);
    CHECK_OFFSET(yfIpfixFlow_t,protocolIdentifier);
    CHECK_OFFSET(yfIpfixFlow_t,flowEndReason);
    CHECK_OFFSET(yfIpfixFlow_t,ingressInterface);
    CHECK_OFFSET(yfIpfixFlow_t,egressInterface);
    CHECK_OFFSET(yfIpfixFlow_t,sourceMacAddress);
    CHECK_OFFSET(yfIpfixFlow_t,destinationMacAddress);
    CHECK_OFFSET(yfIpfixFlow_t,vlanId);
    CHECK_OFFSET(yfIpfixFlow_t,minimumTTL);
    CHECK_OFFSET(yfIpfixFlow_t,maximumTTL);
    CHECK_OFFSET(yfIpfixFlow_t,reverseMinimumTTL);
    CHECK_OFFSET(yfIpfixFlow_t,reverseMaximumTTL);
    CHECK_OFFSET(yfIpfixFlow_t,initialTCPFlags);
    CHECK_OFFSET(yfIpfixFlow_t,reverseInitialTCPFlags);
    CHECK_OFFSET(yfIpfixFlow_t,unionTCPFlags);
    CHECK_OFFSET(yfIpfixFlow_t,reverseUnionTCPFlags);
    CHECK_OFFSET(yfIpfixFlow_t,tcpControlBits);
    CHECK_OFFSET(yfIpfixFlow_t,reverseTcpControlBits);

#undef EO_STRING
#undef CHECK_OFFSET
}


void yfWriterForceBiflowExport(
    gboolean            biforce_mode)
{
    yaf_core_force_biflow = biforce_mode;
}


void yfWriterExportMappedV6(
    gboolean            map_mode)
{
    yaf_core_map_ipv6 = map_mode;
}

void yfWriterUseInterfaceMap(
   qfIfMap_t            *ifmap)
{
    yaf_core_ifmap = ifmap;
}

void yfWriterUseSourceNets(qfNetList_t *srclist)
{
    yaf_source_netlist = srclist;
}

void yfWriterUseSourceMacs(qfMacList_t *maclist)
{
    yaf_source_maclist = maclist;
}

void yfWriterExportReset() {
    qof_export_spec_count = 0;
    memset(qof_export_spec, 0, sizeof(qof_export_spec));
}

gboolean yfWriterExportIE(const char *iename, GError **err) {
    int i;
    gboolean rv = FALSE;
    
    /* shortcircuit if we're full */
    if (qof_export_spec_count >= QOF_EXPORT_SPEC_SZ - 1) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                    "too many elements in export template");
        return FALSE;
    }
    
    /* search the internal spec for the entry to copy */
    /* copy all matches to handle FLE/RLE */
    for (i = 0;
         qof_internal_spec[i].name &&
         (qof_export_spec_count < QOF_EXPORT_SPEC_SZ - 1);
         i++)
    {
        if (strcmp(qof_internal_spec[i].name, iename) == 0) {
            /* match, copy */
            memcpy(&qof_export_spec[qof_export_spec_count],
                   &qof_internal_spec[i], sizeof(fbInfoElementSpec_t));
            qof_export_spec_count++;
            rv = TRUE;
        }
    }
    
    /* set error if we didn't find the requested IE */
    if (!rv) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                    "qof can't export %s", iename);
    }
    
    return rv;
}

size_t yfWriterExportIECount() {
    return qof_export_spec_count;
}

/**
 * yfFlowPrepare
 *
 * initialize the state of a flow to be "clean" so that it
 * can be reused
 *
 */
void yfFlowPrepare(
    yfFlow_t          *flow)
{
    memset(flow, 0, sizeof(*flow));
}

/**
 * yfInfoModel
 *
 *
 */
static fbInfoModel_t *yfInfoModel()
{
    static fbInfoModel_t *yaf_model = NULL;

    if (!yaf_model) {
        yaf_model = fbInfoModelAlloc();
        fbInfoModelAddElementArray(yaf_model, yaf_cert_info_elements);
        fbInfoModelAddElementArray(yaf_model, yaf_tch_info_elements);
        fbInfoModelAddElementArray(yaf_model, yaf_iana_extra_info_elements);
    }

    return yaf_model;
}

static fbTemplate_t *yfAllocInternalTemplate(GError **err) {
    fbInfoModel_t   *model = yfInfoModel();
    fbTemplate_t    *tmpl = fbTemplateAlloc(model);
    
    if (!fbTemplateAppendSpecArray(tmpl, qof_internal_spec, YTF_ALL, err)) {
        fbTemplateFreeUnused(tmpl);
        return NULL;
    }
    
    return tmpl;
}

static fbTemplate_t *yfAllocExportTemplate(uint32_t flags, GError **err) {
    fbInfoModel_t   *model = yfInfoModel();
    fbTemplate_t    *tmpl = fbTemplateAlloc(model);
    
    fbInfoElementSpec_t *spec = qof_export_spec_count ?
                                qof_export_spec : qof_internal_spec;
    
    if (!fbTemplateAppendSpecArray(tmpl, spec, flags, err)) {
        if (tmpl) fbTemplateFreeUnused(tmpl);
        return NULL;
    }

    return tmpl;
}

/**
 * yfInitExporterSession
 *
 * Initializes base templates 
 * (others are created on demand by yfSetExportTemplate)
 */

static fbSession_t *yfInitExporterSession(
    uint32_t        domain,
    GError          **err)
{
    fbInfoModel_t   *model = yfInfoModel();
    fbTemplate_t    *tmpl = NULL;
    fbSession_t     *session = NULL;
    time_t          cur_time = time(NULL);

    yaf_start_time = cur_time * 1000;

    /* Allocate the session */
    session = fbSessionAlloc(model);

    /* set observation domain */
    fbSessionSetDomain(session, domain);

    /* create the (internal) full record template */
    if (!(tmpl = yfAllocInternalTemplate(err))) return NULL;
    
    /* Add the full record template to the session */
    if (!fbSessionAddTemplate(session, TRUE, YAF_FLOW_FULL_TID, tmpl, err)) {
        return NULL;
    }
//    if (!fbSessionAddTemplate(session, FALSE, YAF_FLOW_FULL_TID, tmpl, err)) {
//        return NULL;
//    }

    /* Create the Statistics Template */
    /* FIXME check that the template looks like the structure */
    tmpl = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(tmpl, yaf_stats_option_spec, 0, err))
    {
        return NULL;
    }

    /* Scope fields are exporterIPv4Address and exportingProcessID */
    fbTemplateSetOptionsScope(tmpl, 2);
    if (!fbSessionAddTemplate(session, TRUE, YAF_OPTIONS_TID, tmpl, err))
    {
        return NULL;
    }
    if (!fbSessionAddTemplate(session, FALSE, YAF_OPTIONS_TID, tmpl, err))
    {
        return NULL;
    }

    /* Done. Return the session. */
    return session;
}

/**
 *yfWriterForFile
 *
 *
 */
fBuf_t *yfWriterForFile(
    const char              *path,
    uint32_t                domain,
    GError                  **err)
{
    fBuf_t                  *fbuf = NULL;
    fbExporter_t            *exporter;
    fbSession_t             *session;

    /* Allocate an exporter for the file */
    exporter = fbExporterAllocFile(path);

    /* Create a new buffer */
    if (!(session = yfInitExporterSession(domain, err))) goto err;

    fbuf = fBufAllocForExport(session, exporter);

    /* write YAF flow templates */
    if (!fbSessionExportTemplates(session, err)) goto err;

    /* set internal template */
    if (!fBufSetInternalTemplate(fbuf, YAF_FLOW_FULL_TID, err)) goto err;
    /* all done */
    return fbuf;

  err:
    /* free buffer if necessary */
    if (fbuf) fBufFree(fbuf);
    return NULL;
}

/**
 *yfWriterForFP
 *
 *
 *
 */
fBuf_t *yfWriterForFP(
    FILE                    *fp,
    uint32_t                domain,
    GError                  **err)
{
    fBuf_t                  *fbuf = NULL;
    fbExporter_t            *exporter;
    fbSession_t             *session;

    /* Allocate an exporter for the file */
    exporter = fbExporterAllocFP(fp);
    /* Create a new buffer */
    if (!(session = yfInitExporterSession(domain, err))) goto err;

    fbuf = fBufAllocForExport(session, exporter);

    /* write YAF flow templates */

    if (!fbSessionExportTemplates(session, err)) goto err;

    /* set internal template */
    if (!fBufSetInternalTemplate(fbuf, YAF_FLOW_FULL_TID, err)) goto err;

    /* all done */
    return fbuf;

  err:
    /* free buffer if necessary */
    if (fbuf) fBufFree(fbuf);
    return NULL;
}

/**
 *yfWriterForSpec
 *
 *
 *
 */
fBuf_t *yfWriterForSpec(
    fbConnSpec_t            *spec,
    uint32_t                domain,
    GError                  **err)
{
    fBuf_t                  *fbuf = NULL;
    fbSession_t             *session;
    fbExporter_t            *exporter;

    /* initialize session and exporter */
    if (!(session = yfInitExporterSession(domain, err))) goto err;

    exporter = fbExporterAllocNet(spec);
    fbuf = fBufAllocForExport(session, exporter);

    /* set observation domain */
    fbSessionSetDomain(session, domain);

    /* write YAF flow templates */
    if (!fbSessionExportTemplates(session, err)) goto err;

    /* set internal template */
    if (!fBufSetInternalTemplate(fbuf, YAF_FLOW_FULL_TID, err)) goto err;

    /* all done */
    return fbuf;

  err:
    /* free buffer if necessary */
    if (fbuf) fBufFree(fbuf);
    return NULL;
}

/**
 *yfSetExportTemplate
 *
 *
 *
 */
static gboolean yfSetExportTemplate(
    fBuf_t              *fbuf,
    uint16_t            tid,
    GError              **err)
{
    fbSession_t         *session = NULL;
    fbTemplate_t        *tmpl = NULL;


    /* Try to set export template */
    if (fBufSetExportTemplate(fbuf, tid, err)) {
        return TRUE;
    }

    /* Check for error other than missing template */
    if (!g_error_matches(*err, FB_ERROR_DOMAIN, FB_ERROR_TMPL)) {
        return FALSE;
    }

    /* Okay. We have a missing template. Clear the error and try to load it. */
    g_clear_error(err);
    session = fBufGetSession(fbuf);
    
    if (!(tmpl = yfAllocExportTemplate(tid & (~YAF_FLOW_BASE_TID), err))) {
        return FALSE;
    }
    
    if (!fbSessionAddTemplate(session, FALSE, tid, tmpl, err)) {
        return FALSE;
    }

    /* Template should be loaded. Try setting the template again. */
    return fBufSetExportTemplate(fbuf, tid, err);
}

/**
 *yfWriteStatsRec
 *
 *
 */
gboolean yfWriteStatsRec(
    void            *qfctx,
    GError          **err)
{
    yfIpfixStats_t      rec;
    qfContext_t         *ctx = (qfContext_t *)qfctx;
    fBuf_t              *fbuf = ctx->octx.fbuf;
    uint32_t            mask = 0x000000FF;
    char                buf[200];
    static struct hostent *host;
    static uint32_t     host_ip = 0;

    yfGetFlowTabStats(ctx->flowtab, &(rec.packetTotalCount),
                      &(rec.exportedFlowTotalCount),
                      &(rec.notSentPacketTotalCount),
                      &(rec.flowTablePeakCount),
                      &(rec.flowTableFlushEvents));
    if (ctx->fragtab) {
        yfGetFragTabStats(ctx->fragtab,
                          &(rec.expiredFragmentCount),
                          &(rec.assembledFragmentCount));
    } else {
        rec.expiredFragmentCount = 0;
        rec.assembledFragmentCount = 0;
    }

    if (!fbuf) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "no output available for stats record");
        return FALSE;
    }

    /* Get IP of sensor for scope */
    if (!host) {
        gethostname(buf, 200);
        host = (struct hostent *)gethostbyname(buf);
        if (host) {
            host_ip = (host->h_addr[0] & mask)  << 24;
            host_ip |= (host->h_addr[1] & mask) << 16;
            host_ip |= (host->h_addr[2] & mask) << 8;
            host_ip |= (host->h_addr[3] & mask);
        }
    }

    /* Rejected/Ignored Packet Total Count from decode.c */
    rec.ignoredPacketTotalCount = yfDecodeUndecodedCount(ctx->dectx);

    /* Dropped packets - from yafcap.c & libpcap */
    rec.droppedPacketTotalCount = yfStatGetDropped();
    rec.exporterIPv4Address = host_ip;

    /* Use Observation ID as exporting Process ID */
    rec.exportingProcessId = ctx->octx.odid;

    rec.systemInitTimeMilliseconds = yaf_start_time;
    
    /* Set Internal Template for Buffer to Options TID */
    if (!fBufSetInternalTemplate(fbuf, YAF_OPTIONS_TID, err))
        return FALSE;

    /* Set Export Template for Buffer to Options TMPL */
    if (!yfSetExportTemplate(fbuf, YAF_OPTIONS_TID, err)) {
        return FALSE;
    }

    /* Append Record */
    if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
        return FALSE;
    }

    /* Set Internal TID Back to Flow Record */
    if (!fBufSetInternalTemplate(fbuf, YAF_FLOW_FULL_TID, err)) {
        return FALSE;
    }

    return TRUE;
}
 
/**
 *yfWriteFlow
 *
 *
 *
 */
gboolean yfWriteFlow(
    fBuf_t              *fbuf,
    yfFlow_t            *flow,
    GError              **err)
{
    yfIpfixFlow_t       rec;
    uint16_t            wtid;
    uint32_t            hz, rhz;
    
    yfFlowVal_t         *val, *rval;
    yfFlowKey_t         kbuf, *key;
    
    /* assign flow directions */
    if ((yaf_source_netlist &&
        (qfFlowDirection(yaf_source_netlist, &flow->key) == QF_DIR_OUT)) ||
        (yaf_source_maclist &&
         qfMacListContains(yaf_source_maclist, flow->destinationMacAddr)))
    {
        yfFlowKeyReverse(&flow->key, &kbuf);
        key = &kbuf;
        val = &flow->rval;
        rval = &flow->val;
    } else {
        key = &flow->key;
        val = &flow->val;
        rval = &flow->rval;
    }
    
    /* copy time */
    rec.flowStartMilliseconds = flow->stime;
    rec.flowEndMilliseconds = flow->etime;
    rec.reverseFlowDeltaMilliseconds = flow->rdtime;

    /* choose options for basic template */
    wtid = YAF_FLOW_BASE_TID;

    /* fill in fields that are always present */
    rec.flowId = flow->fid;
    rec.flowEndReason = flow->reason;
    rec.minimumTTL = val->minttl;
    rec.reverseMinimumTTL = rval->minttl;
    rec.maximumTTL = val->maxttl;
    rec.reverseMaximumTTL = rval->maxttl;
    rec.octetCount = val->oct;
    rec.reverseOctetCount = rval->oct;
    rec.packetCount = val->pkt;
    rec.reversePacketCount = rval->pkt;
    rec.transportOctetDeltaCount = val->appoct;
    rec.reverseTransportOctetDeltaCount = rval->appoct;
    rec.transportPacketDeltaCount = val->apppkt;
    rec.reverseTransportPacketDeltaCount = rval->apppkt;
    
    /* set ingress and egress interface from map if not already set */
    if (yaf_core_ifmap) {
        qfIfMapAddresses(yaf_core_ifmap, &flow->key,
                         &val->netIf, &rval->netIf);
    }
    
    /* copy ports and protocol */
    rec.sourceTransportPort = key->sp;
    rec.destinationTransportPort = key->dp;
    rec.protocolIdentifier = key->proto;
        
    /* copy addresses */
    if (yaf_core_map_ipv6 && (key->version == 4)) {
        memcpy(rec.sourceIPv6Address, yaf_ip6map_pfx,
               sizeof(yaf_ip6map_pfx));
        *(uint32_t *)(&(rec.sourceIPv6Address[sizeof(yaf_ip6map_pfx)])) =
            g_htonl(key->addr.v4.sip);
        memcpy(rec.destinationIPv6Address, yaf_ip6map_pfx,
               sizeof(yaf_ip6map_pfx));
        *(uint32_t *)(&(rec.destinationIPv6Address[sizeof(yaf_ip6map_pfx)])) =
            g_htonl(key->addr.v4.dip);
        wtid |= YTF_IP6;
    } else if (key->version == 4) {
        rec.sourceIPv4Address = key->addr.v4.sip;
        rec.destinationIPv4Address = key->addr.v4.dip;
        wtid |= YTF_IP4;
    } else if (key->version == 6) {
        memcpy(rec.sourceIPv6Address, key->addr.v6.sip,
               sizeof(rec.sourceIPv6Address));
        memcpy(rec.destinationIPv6Address, key->addr.v6.dip,
               sizeof(rec.destinationIPv6Address));
        wtid |= YTF_IP6;
    } else {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                    "Illegal IP version %u", key->version);
    }
    
    /* TCP flow; copy TCP data and enable export */
    if (key->proto == YF_PROTO_TCP) {
        wtid |= YTF_TCP;

        rec.initialTCPFlags = val->iflags;
        rec.reverseInitialTCPFlags = rval->iflags;
        rec.unionTCPFlags = val->uflags;
        rec.reverseUnionTCPFlags = rval->uflags;
        rec.tcpControlBits = val->iflags | val->uflags;
        rec.reverseTcpControlBits = rval->iflags | rval->uflags;
        
        if (val->tcp) {
            rec.tcpSequenceCount = qfSeqCount(&val->tcp->seq,
                                              val->iflags | val->uflags);
            rec.tcpSequenceLossCount = qfSeqCountLost(&val->tcp->seq);
            rec.tcpRetransmitCount = val->tcp->seq.rtx;
            rec.tcpSequenceJumpCount = val->tcp->seq.ooo;
            rec.maxTcpSequenceJump = val->tcp->seq.maxooo;
            rec.tcpLossEventCount = val->tcp->seq.lossct;
            rec.tcpDupAckCount = val->tcp->ack.dup_ct;
            rec.tcpSelAckCount = val->tcp->ack.sel_ct;
            rec.qofTcpCharacteristics = val->tcp->opts.flags;
            rec.tcpSequenceNumber = val->tcp->seq.isn;
            rec.observedTcpMss = val->tcp->opts.mss;
            rec.declaredTcpMss = val->tcp->opts.mss_opt;
            rec.minTcpRwin = val->tcp->rwin.val.mm.min;
            rec.meanTcpRwin = (uint32_t)val->tcp->rwin.val.mean;
            rec.maxTcpRwin = val->tcp->rwin.val.mm.max;
            rec.tcpReceiverStallCount = val->tcp->rwin.stall;
            rec.minTcpIOTMilliseconds = val->tcp->seq.seg_iat.mm.min;
            rec.maxTcpIOTMilliseconds = val->tcp->seq.seg_iat.mm.max;
        }
        
        if (rval->tcp) {
            rec.reverseTcpSequenceCount = qfSeqCount(&rval->tcp->seq,
                                          rval->iflags | rval->uflags);
            rec.reverseTcpSequenceLossCount = qfSeqCountLost(&rval->tcp->seq);
            rec.reverseTcpRetransmitCount = rval->tcp->seq.rtx;
            rec.reverseTcpSequenceJumpCount = rval->tcp->seq.ooo;
            rec.reverseMaxTcpSequenceJump = rval->tcp->seq.maxooo;
            rec.reverseTcpLossEventCount = rval->tcp->seq.lossct;
            rec.reverseTcpDupAckCount = rval->tcp->ack.dup_ct;
            rec.reverseTcpSelAckCount = rval->tcp->ack.sel_ct;
            rec.reverseQofTcpCharacteristics = rval->tcp->opts.flags;
            rec.reverseTcpSequenceNumber = rval->tcp->seq.isn;
            rec.reverseObservedTcpMss = rval->tcp->opts.mss;
            rec.reverseDeclaredTcpMss = rval->tcp->opts.mss_opt;
            rec.reverseMinTcpRwin = rval->tcp->rwin.val.mm.min;
            rec.reverseMeanTcpRwin = (uint32_t)rval->tcp->rwin.val.mean;
            rec.reverseMaxTcpRwin = rval->tcp->rwin.val.mm.max;
            rec.reverseTcpReceiverStallCount = rval->tcp->rwin.stall;
            rec.reverseMinTcpIOTMilliseconds = rval->tcp->seq.seg_iat.mm.min;
            rec.reverseMaxTcpIOTMilliseconds = rval->tcp->seq.seg_iat.mm.max;
        }
        
        if (val->tcp && rval->tcp &&
            ((hz = qfTimestampHz(&val->tcp->seq)) ||
            (rhz = qfTimestampHz(&rval->tcp->seq))))
        {
            wtid |= YTF_TSV;
            rec.tcpTimestampFrequency = qfTimestampHz(&val->tcp->seq);
            rec.reverseTcpTimestampFrequency =  qfTimestampHz(&rval->tcp->seq);
            rec.minTcpChirpMilliseconds = (int16_t)val->tcp->seq.seg_variat.mm.min;
            rec.reverseMinTcpChirpMilliseconds = (int16_t)rval->tcp->seq.seg_variat.mm.min;
            rec.maxTcpChirpMilliseconds = (int16_t)val->tcp->seq.seg_variat.mm.max;
            rec.reverseMaxTcpChirpMilliseconds = (int16_t)rval->tcp->seq.seg_variat.mm.max;

        }

        /* Enable RTT export if we have enough samples */
        if (flow->rtt.val.n >= QOF_MIN_RTT_COUNT) {
            wtid |= YTF_RTT;
            rec.lastTcpRttMilliseconds = flow->rtt.val.val;
            rec.minTcpRttMilliseconds = flow->rtt.val.mm.min;
            rec.tcpRttSampleCount = flow->rtt.val.n;
        }
    }
    
    /* MAC layer information */
    memcpy(rec.sourceMacAddress, flow->sourceMacAddr,
           ETHERNET_MAC_ADDR_LENGTH);
    memcpy(rec.destinationMacAddress, flow->destinationMacAddr,
           ETHERNET_MAC_ADDR_LENGTH);
    rec.vlanId = key->vlanId;
    
    rec.ingressInterface = val->netIf;
    rec.egressInterface = rval->netIf;

    /* Set flags based on exported record properties */
    
    /* Set biflow flag */
    if (yaf_core_force_biflow || rec.reversePacketCount) {
        wtid |= YTF_BIF;
    }
    
    /* Set RLE flag */
    if (rec.octetCount < YAF_RLEMAX &&
        rec.reverseOctetCount < YAF_RLEMAX &&
        rec.packetCount < YAF_RLEMAX &&
        rec.reversePacketCount < YAF_RLEMAX) {
        wtid |= YTF_RLE;
    } else {
        wtid |= YTF_FLE;
    }

    /* Select template and export */
    if (!yfSetExportTemplate(fbuf, wtid, err)) {
        return FALSE;
    }

    /* FIXME where'd UDP template retransmit go? */
    
    /* Now append the record to the buffer */
    if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
        return FALSE;
    }
    
    return TRUE;
}

/**
 *yfWriterClose
 *
 *
 *
 */
gboolean yfWriterClose(
    fBuf_t          *fbuf,
    gboolean        flush,
    GError          **err)
{
    gboolean        ok = TRUE;

    if (flush) {
        ok = fBufEmit(fbuf, err);
    }

    fBufFree(fbuf);

    return ok;
}

#if 0
/**
 * yfTemplateCallback
 *
 *
 */
static void yfTemplateCallback(
    fbSession_t     *session,
    uint16_t        tid,
    fbTemplate_t    *tmpl)
{
    uint16_t ntid;

    ntid = tid & YTF_REV;

    if (YAF_FLOW_BASE_TID == (tid & 0xF000)) {
        fbSessionAddTemplatePair(session, tid, tid);
    }

    if (ntid == YAF_ENTROPY_FLOW_TID) {
        fbSessionAddTemplatePair(session, tid, tid);
    } else if (ntid == YAF_TCP_FLOW_TID) {
        fbSessionAddTemplatePair(session, tid, tid);
    } else if (ntid == YAF_MAC_FLOW_TID) {
        fbSessionAddTemplatePair(session, tid, tid);
    } else if (ntid == YAF_PAYLOAD_FLOW_TID) {
        fbSessionAddTemplatePair(session, tid, tid);
    } else {
        /* Dont decode templates yafscii doesn't care about */
        fbSessionAddTemplatePair(session, tid, 0);
    }

}
#endif

#if 0
/**
 *yfInitCollectorSession
 *
 *
 *
 */
static fbSession_t *yfInitCollectorSession(
    GError          **err)
{
    fbInfoModel_t   *model = yfInfoModel();
    fbTemplate_t    *tmpl = NULL;
    fbSession_t     *session = NULL;

    /* Allocate the session */
    session = fbSessionAlloc(model);

    /* Add the full record template */
    if (!(tmpl = yfAllocInternalTemplate(err))) return NULL;
    
    if (!fbSessionAddTemplate(session, TRUE, YAF_FLOW_FULL_TID, tmpl, err))
        return NULL;

    /* Done. Return the session. */
    return session;
}

/**
 *yfReaderForFP
 *
 *
 *
 */
fBuf_t *yfReaderForFP(
    fBuf_t          *fbuf,
    FILE            *fp,
    GError          **err)
{
    fbSession_t     *session;
    fbCollector_t   *collector;

    /* Allocate a collector for the file */
    collector = fbCollectorAllocFP(NULL, fp);

    /* Allocate a buffer, or reset the collector */
    if (fbuf) {
        fBufSetCollector(fbuf, collector);
    } else {
        if (!(session = yfInitCollectorSession(err))) goto err;
        fbuf = fBufAllocForCollection(session, collector);
    }

    /* FIXME do a preread? */

    return fbuf;

  err:
    /* free buffer if necessary */
    if (fbuf) fBufFree(fbuf);
    return NULL;
}

/**
 *yfListenerForSpec
 *
 *
 *
 */
fbListener_t *yfListenerForSpec(
    fbConnSpec_t            *spec,
    fbListenerAppInit_fn    appinit,
    fbListenerAppFree_fn    appfree,
    GError                  **err)
{
    fbSession_t     *session;

    if (!(session = yfInitCollectorSession(err))) return NULL;

    return fbListenerAlloc(spec, session, appinit, appfree, err);
}


/**
 *yfReadFlow
 *
 * read an IPFIX record in, with respect to fields YAF cares about
 *
 */
     
/* FIXME review all this when we rewrite yafscii to support tcp hackery */

gboolean yfReadFlow(
    fBuf_t          *fbuf,
    yfFlow_t        *flow,
    GError          **err)
{
    yfIpfixFlow_t       rec;
    size_t              len;

    fbTemplate_t        *next_tmpl = NULL;

    len = sizeof(yfIpfixFlow_t);

    /* Check if Options Template - if so - ignore */
    next_tmpl = fBufNextCollectionTemplate(fbuf, NULL, err);
    if (next_tmpl) {
        if (fbTemplateGetOptionsScope(next_tmpl)) {
            /* Stats Msg - Don't actually Decode */
            if (!fBufNext(fbuf, (uint8_t *)&rec, &len, err)) {
                return FALSE;
            }
            return TRUE;
        }
    } else {
        return FALSE;
    }

    /* read next YAF record */
    if (!fBufSetInternalTemplate(fbuf, YAF_FLOW_FULL_TID, err))
        return FALSE;
    if (!fBufNext(fbuf, (uint8_t *)&rec, &len, err))
        return FALSE;

    /* copy time */
    flow->stime = rec.flowStartMilliseconds;
    flow->etime = rec.flowEndMilliseconds;
    flow->rdtime = rec.reverseFlowDeltaMilliseconds;
    /* copy addresses */
    if (rec.sourceIPv4Address || rec.destinationIPv4Address) {
        flow->key.version = 4;
        flow->key.addr.v4.sip = rec.sourceIPv4Address;
        flow->key.addr.v4.dip = rec.destinationIPv4Address;
    } else if (rec.sourceIPv6Address || rec.destinationIPv6Address) {
        flow->key.version = 6;
        memcpy(flow->key.addr.v6.sip, rec.sourceIPv6Address,
               sizeof(flow->key.addr.v6.sip));
        memcpy(flow->key.addr.v6.dip, rec.destinationIPv6Address,
               sizeof(flow->key.addr.v6.dip));
    } else {
        /* Hmm. Default to v4 null addressing for now. */
        flow->key.version = 4;
        flow->key.addr.v4.sip = 0;
        flow->key.addr.v4.dip = 0;
    }

    /* copy key and counters */
    flow->key.sp = rec.sourceTransportPort;
    flow->key.dp = rec.destinationTransportPort;
    flow->key.proto = rec.protocolIdentifier;
    flow->val.oct = rec.octetCount;
    flow->val.pkt = rec.packetCount;
    if (flow->val.oct == 0 && flow->val.pkt == 0) {
        flow->val.oct = rec.octetCount;
        flow->val.pkt = rec.packetCount;
    }
    flow->key.vlanId = rec.vlanId;
    flow->rval.oct = rec.reverseOctetCount;
    flow->rval.pkt = rec.reversePacketCount;
    flow->reason = rec.flowEndReason;

    flow->val.isn = rec.tcpSequenceNumber;
    flow->val.iflags = rec.initialTCPFlags;
    flow->val.uflags = rec.unionTCPFlags;
    flow->rval.isn = rec.reverseTcpSequenceNumber;
    flow->rval.iflags = rec.reverseInitialTCPFlags;
    flow->rval.uflags = rec.reverseUnionTCPFlags;

    return TRUE;
}

/**
 *yfNTPDecode
 *
 * decodes a 64-bit NTP time variable and returns it in terms of
 * milliseconds
 *
 *
 */
static uint64_t yfNTPDecode(
    uint64_t        ntp)
{
    double          dntp;
    uint64_t        millis;

    if (!ntp) return 0;

    dntp = (ntp & 0xFFFFFFFF00000000LL) >> 32;
    dntp += ((ntp & 0x00000000FFFFFFFFLL) * 1.0) / (2LL << 32);
    millis = dntp * 1000;
    return millis;
}
#endif

/* this is basically for interop testing. drop for now, 
   since we don't have an extflow anymore... */
#if 0
/**
 *yfReadFlowExtended
 *
 * read an IPFIX flow record in (with respect to fields YAF cares about)
 * using YAF's extended precision time recording
 *
 */
gboolean yfReadFlowExtended(
    fBuf_t                  *fbuf,
    yfFlow_t                *flow,
    GError                  **err)
{
    yfIpfixExtFlow_t        rec;
    fbTemplate_t            *next_tmpl = NULL;
    size_t                  len;

    yfTcpFlow_t         *tcprec = NULL;
    yfMacFlow_t         *macrec = NULL;

    /* read next YAF record; retrying on missing template or EOF. */
    len = sizeof(yfIpfixExtFlow_t);
    if (!fBufSetInternalTemplate(fbuf, YAF_FLOW_EXT_TID, err))
        return FALSE;

    while (1) {

        /* Check if Options Template - if so - ignore */
        next_tmpl = fBufNextCollectionTemplate(fbuf, NULL, err);
        if (next_tmpl) {
            if (fbTemplateGetOptionsScope(next_tmpl)) {
                if (!(fBufNext(fbuf, (uint8_t *)&rec, &len, err))) {
                    return FALSE;
                }
                continue;
            }
        } else {
            return FALSE;
        }
        if (fBufNext(fbuf, (uint8_t *)&rec, &len, err)) {
            break;
        } else {
            if (g_error_matches(*err, FB_ERROR_DOMAIN, FB_ERROR_TMPL)) {
                /* try again on missing template */
                g_debug("skipping IPFIX data set: %s", (*err)->message);
                g_clear_error(err);
                continue;
            } else {
                /* real, actual error */
                return FALSE;
            }
        }
    }

    /* Run the Gauntlet of Time. */
    if (rec.f.flowStartMilliseconds) {
        flow->stime = rec.f.flowStartMilliseconds;
        if (rec.f.flowEndMilliseconds >= rec.f.flowStartMilliseconds) {
            flow->etime = rec.f.flowEndMilliseconds;
        } else {
            flow->etime = flow->stime + rec.flowDurationMilliseconds;
        }
    } else if (rec.flowStartMicroseconds) {
        /* Decode NTP-format microseconds */
        flow->stime = yfNTPDecode(rec.flowStartMicroseconds);
        if (rec.flowEndMicroseconds >= rec.flowStartMicroseconds) {
            flow->etime = yfNTPDecode(rec.flowEndMicroseconds);
        } else {
            flow->etime = flow->stime + (rec.flowDurationMicroseconds / 1000);
        }
    } else if (rec.flowStartSeconds) {
        /* Seconds? Well. Okay... */
        flow->stime = rec.flowStartSeconds * 1000;
        flow->etime = rec.flowEndSeconds * 1000;
    } else if (rec.flowStartDeltaMicroseconds) {
        /* Handle delta microseconds. */
        flow->stime = fBufGetExportTime(fbuf) * 1000 -
                      rec.flowStartDeltaMicroseconds / 1000;
        if (rec.flowEndDeltaMicroseconds &&
            rec.flowEndDeltaMicroseconds <= rec.flowStartDeltaMicroseconds) {
            flow->etime = fBufGetExportTime(fbuf) * 1000 -
                          rec.flowEndDeltaMicroseconds / 1000;
        } else {
            flow->etime = flow->stime + (rec.flowDurationMicroseconds / 1000);
        }
    } else {
        /* Out of time. Use current timestamp, zero duration */
        struct timeval ct;
        g_assert(!gettimeofday(&ct, NULL));
        flow->stime = ((uint64_t)ct.tv_sec * 1000) +
                      ((uint64_t)ct.tv_usec / 1000);
        flow->etime = flow->stime;
    }

    /* copy private time field - reverse delta */
    flow->rdtime = rec.f.reverseFlowDeltaMilliseconds;

    /* copy addresses */
    if (rec.f.sourceIPv4Address || rec.f.destinationIPv4Address) {
        flow->key.version = 4;
        flow->key.addr.v4.sip = rec.f.sourceIPv4Address;
        flow->key.addr.v4.dip = rec.f.destinationIPv4Address;
    } else if (rec.f.sourceIPv6Address || rec.f.destinationIPv6Address) {
        flow->key.version = 6;
        memcpy(flow->key.addr.v6.sip, rec.f.sourceIPv6Address,
               sizeof(flow->key.addr.v6.sip));
        memcpy(flow->key.addr.v6.dip, rec.f.destinationIPv6Address,
               sizeof(flow->key.addr.v6.dip));
    } else {
        /* Hmm. Default to v4 null addressing for now. */
        flow->key.version = 4;
        flow->key.addr.v4.sip = 0;
        flow->key.addr.v4.dip = 0;
    }

    /* copy key and counters */
    flow->key.sp = rec.f.sourceTransportPort;
    flow->key.dp = rec.f.destinationTransportPort;
    flow->key.proto = rec.f.protocolIdentifier;
    flow->val.oct = rec.f.octetTotalCount;
    flow->val.pkt = rec.f.packetTotalCount;
    flow->rval.oct = rec.f.reverseOctetTotalCount;
    flow->rval.pkt = rec.f.reversePacketTotalCount;
    flow->key.vlanId = rec.f.vlanId;
    flow->reason = rec.f.flowEndReason;
    /* Handle delta counters */
    if (!(flow->val.oct)) {
        flow->val.oct = rec.f.octetDeltaCount;
        flow->rval.oct = rec.f.reverseOctetDeltaCount;
    }
    if (!(flow->val.pkt)) {
        flow->val.pkt = rec.f.packetDeltaCount;
        flow->rval.pkt = rec.f.reversePacketDeltaCount;
    }

    flow->val.isn = rec.f.tcpSequenceNumber;
    flow->val.iflags = rec.f.initialTCPFlags;
    flow->val.uflags = rec.f.unionTCPFlags;
    flow->rval.isn = rec.f.reverseTcpSequenceNumber;
    flow->rval.iflags = rec.f.reverseInitialTCPFlags;
    flow->rval.uflags = rec.f.reverseUnionTCPFlags;


    return TRUE;
}
#endif 


/**
 *yfPrintFlags
 *
 *
 *
 */
static void yfPrintFlags(
    GString             *str,
    uint8_t             flags)
{
    if (flags & YF_TF_ECE) g_string_append_c(str, 'E');
    if (flags & YF_TF_CWR) g_string_append_c(str, 'C');
    if (flags & YF_TF_URG) g_string_append_c(str, 'U');
    if (flags & YF_TF_ACK) g_string_append_c(str, 'A');
    if (flags & YF_TF_PSH) g_string_append_c(str, 'P');
    if (flags & YF_TF_RST) g_string_append_c(str, 'R');
    if (flags & YF_TF_SYN) g_string_append_c(str, 'S');
    if (flags & YF_TF_FIN) g_string_append_c(str, 'F');
    if (!flags) g_string_append_c(str, '0');
}

/**
 *yfPrintString
 *
 *
 *
 */
void yfPrintString(
    GString             *rstr,
    yfFlow_t            *flow)
{
    char                sabuf[AIR_IP6ADDR_BUF_MINSZ],
                        dabuf[AIR_IP6ADDR_BUF_MINSZ];

    /* print start as date and time */
    air_mstime_g_string_append(rstr, flow->stime, AIR_TIME_ISO8601);

    /* print end as time and duration if not zero-duration */
    if (flow->stime != flow->etime) {
        g_string_append_printf(rstr, " - ");
        air_mstime_g_string_append(rstr, flow->etime, AIR_TIME_ISO8601_HMS);
        g_string_append_printf(rstr, " (%.3f sec)",
            (flow->etime - flow->stime) / 1000.0);
    }

    /* print protocol and addresses */
    if (flow->key.version == 4) {
        air_ipaddr_buf_print(sabuf, flow->key.addr.v4.sip);
        air_ipaddr_buf_print(dabuf, flow->key.addr.v4.dip);
    } else if (flow->key.version == 6) {
        air_ip6addr_buf_print(sabuf, flow->key.addr.v6.sip);
        air_ip6addr_buf_print(dabuf, flow->key.addr.v6.dip);
    } else {
        sabuf[0] = (char)0;
        dabuf[0] = (char)0;
    }

    switch (flow->key.proto) {
    case YF_PROTO_TCP:
        if (flow->rval.oct) {
            g_string_append_printf(rstr, " tcp %s:%u => %s:%u ",
                                   sabuf, flow->key.sp, dabuf, flow->key.dp);
        } else {
            g_string_append_printf(rstr, " tcp %s:%u => %s:%u ",
                                   sabuf, flow->key.sp, dabuf, flow->key.dp);
        }

        yfPrintFlags(rstr, flow->val.iflags);
        g_string_append_c(rstr,'/');
        yfPrintFlags(rstr, flow->val.uflags);
        if (flow->rval.oct) {
            g_string_append_c(rstr,':');
            yfPrintFlags(rstr, flow->rval.iflags);
            g_string_append_c(rstr,'/');
            yfPrintFlags(rstr, flow->rval.uflags);
        }
        break;
    case YF_PROTO_UDP:
        g_string_append_printf(rstr, " udp %s:%u => %s:%u",
                               sabuf, flow->key.sp, dabuf, flow->key.dp);
        break;
    case YF_PROTO_ICMP:
        g_string_append_printf(rstr, " icmp [%u:%u] %s => %s",
                               (flow->key.dp >> 8), (flow->key.dp & 0xFF),
                               sabuf, dabuf);
        break;
    case YF_PROTO_ICMP6:
        g_string_append_printf(rstr, " icmp6 [%u:%u] %s => %s",
                               (flow->key.dp >> 8), (flow->key.dp & 0xFF),
                               sabuf, dabuf);
        break;
    default:
        g_string_append_printf(rstr, " ip %u %s => %s",
                               flow->key.proto, sabuf, dabuf);
        break;
    }


    /* print vlan tags */
    if (flow->key.vlanId) {
        if (flow->rval.oct) {
            g_string_append_printf(rstr, " vlan %03hx:%03hx",
                flow->key.vlanId, flow->key.vlanId);
        } else {
            g_string_append_printf(rstr, " vlan %03hx",
                flow->key.vlanId);
        }
    }

    /* print flow counters and round-trip time */
    if (flow->rval.pkt) {
        g_string_append_printf(rstr, " (%llu/%llu <-> %llu/%llu) rtt %u ms",
                               (long long unsigned int)flow->val.pkt,
                               (long long unsigned int)flow->val.oct,
                               (long long unsigned int)flow->rval.pkt,
                               (long long unsigned int)flow->rval.oct,
                               flow->rdtime);
    } else {
        g_string_append_printf(rstr, " (%llu/%llu ->)",
                               (long long unsigned int)flow->val.pkt,
                               (long long unsigned int)flow->val.oct);
    }

    /* end reason flags */
    if ((flow->reason & YAF_END_MASK) == YAF_END_IDLE)
        g_string_append(rstr," idle");
    if ((flow->reason & YAF_END_MASK) == YAF_END_ACTIVE)
        g_string_append(rstr," active");
    if ((flow->reason & YAF_END_MASK) == YAF_END_FORCED)
        g_string_append(rstr," eof");
    if ((flow->reason & YAF_END_MASK) == YAF_END_RESOURCE)
        g_string_append(rstr," rsrc");
    if ((flow->reason & YAF_END_MASK) == YAF_END_UDPFORCE)
        g_string_append(rstr, " force");

    /* finish line */
    g_string_append(rstr,"\n");

}

#if 0
/**
 *yfPrintDelimitedString
 *
 *
 *
 */
void yfPrintDelimitedString(
    GString                 *rstr,
    yfFlow_t                *flow,
    gboolean                yaft_mac)
{
    char                sabuf[AIR_IP6ADDR_BUF_MINSZ],
                        dabuf[AIR_IP6ADDR_BUF_MINSZ];
    GString             *fstr = NULL;
    int                 loop = 0;

    /* print time and duration */
    air_mstime_g_string_append(rstr, flow->stime, AIR_TIME_ISO8601);
    g_string_append_printf(rstr, "%s", YF_PRINT_DELIM);
    air_mstime_g_string_append(rstr, flow->etime, AIR_TIME_ISO8601);
    g_string_append_printf(rstr, "%s%8.3f%s",
        YF_PRINT_DELIM, (flow->etime - flow->stime) / 1000.0, YF_PRINT_DELIM);

    /* print initial RTT */
    g_string_append_printf(rstr, "%8.3f%s",
        flow->rdtime / 1000.0, YF_PRINT_DELIM);

    /* print five tuple */
    if (flow->key.version == 4) {
        air_ipaddr_buf_print(sabuf, flow->key.addr.v4.sip);
        air_ipaddr_buf_print(dabuf, flow->key.addr.v4.dip);
    } else if (flow->key.version == 6) {
        air_ip6addr_buf_print(sabuf, flow->key.addr.v6.sip);
        air_ip6addr_buf_print(dabuf, flow->key.addr.v6.dip);
    } else {
        sabuf[0] = (char)0;
        dabuf[0] = (char)0;

    }
    g_string_append_printf(rstr, "%3u%s%40s%s%5u%s%40s%s%5u%s",
        flow->key.proto, YF_PRINT_DELIM,
        sabuf, YF_PRINT_DELIM, flow->key.sp, YF_PRINT_DELIM,
        dabuf, YF_PRINT_DELIM, flow->key.dp, YF_PRINT_DELIM);

    if (yaft_mac) {
        for (loop = 0; loop < 6; loop++) {
            g_string_append_printf(rstr, "%02x", flow->sourceMacAddr[loop]);
            if (loop < 5) {
                g_string_append_printf(rstr, ":");
            }
            /* clear out mac addr for next flow */
            flow->sourceMacAddr[loop] = 0;
        }
        g_string_append_printf(rstr, "%s", YF_PRINT_DELIM);
        for(loop =0; loop< 6; loop++) {
            g_string_append_printf(rstr, "%02x", flow->destinationMacAddr[loop]);
            if (loop < 5) {
                g_string_append_printf(rstr, ":");
            }
            /* clear out mac addr for next flow */
            flow->destinationMacAddr[loop] = 0;
        }
        g_string_append_printf(rstr, "%s", YF_PRINT_DELIM);
    }

    /* print tcp flags */
    fstr = g_string_new("");
    yfPrintFlags(fstr, flow->val.iflags);
    g_string_append_printf(rstr, "%8s%s", fstr->str, YF_PRINT_DELIM);
    g_string_truncate(fstr, 0);
    yfPrintFlags(fstr, flow->val.uflags);
    g_string_append_printf(rstr, "%8s%s", fstr->str, YF_PRINT_DELIM);
    g_string_truncate(fstr, 0);
    yfPrintFlags(fstr, flow->rval.iflags);
    g_string_append_printf(rstr, "%8s%s", fstr->str, YF_PRINT_DELIM);
    g_string_truncate(fstr, 0);
    yfPrintFlags(fstr, flow->rval.uflags);
    g_string_append_printf(rstr, "%8s%s", fstr->str, YF_PRINT_DELIM);
    g_string_free(fstr, TRUE);

    /* print tcp sequence numbers */
    g_string_append_printf(rstr, "%08x%s%08x%s", flow->val.isn, YF_PRINT_DELIM,
                           flow->rval.isn, YF_PRINT_DELIM);

    /* print vlan tags */
    if (flow->rval.oct) {
        g_string_append_printf(rstr, "%03hx%s%03hx%s", flow->key.vlanId,
                               YF_PRINT_DELIM, flow->key.vlanId,
                               YF_PRINT_DELIM);
    } else {
        g_string_append_printf(rstr, "%03hx%s%03hx%s", flow->key.vlanId,
                               YF_PRINT_DELIM, 0, YF_PRINT_DELIM);
    }


    /* print flow counters */
    g_string_append_printf(rstr, "%8llu%s%8llu%s%8llu%s%8llu%s",
        (long long unsigned int)flow->val.pkt, YF_PRINT_DELIM,
        (long long unsigned int)flow->val.oct, YF_PRINT_DELIM,
        (long long unsigned int)flow->rval.pkt, YF_PRINT_DELIM,
        (long long unsigned int)flow->rval.oct, YF_PRINT_DELIM);

    /* end reason flags */
    if ((flow->reason & YAF_END_MASK) == YAF_END_IDLE)
        g_string_append(rstr,"idle ");
    if ((flow->reason & YAF_END_MASK) == YAF_END_ACTIVE)
        g_string_append(rstr,"active ");
    if ((flow->reason & YAF_END_MASK) == YAF_END_FORCED)
        g_string_append(rstr,"eof ");
    if ((flow->reason & YAF_END_MASK) == YAF_END_RESOURCE)
        g_string_append(rstr,"rsrc ");
    if ((flow->reason & YAF_END_MASK) == YAF_END_UDPFORCE)
        g_string_append(rstr, "force ");


    /* finish line */
    g_string_append(rstr,"\n");

}
#endif

/**
 *yfPrint
 *
 *
 *
 */
gboolean yfPrint(
    FILE                *out,
    yfFlow_t            *flow,
    GError              **err)
{
    GString             *rstr = NULL;
    int                 rc = 0;

    rstr = g_string_new("");

    yfPrintString(rstr, flow);

    rc = fwrite(rstr->str, rstr->len, 1, out);

    if (rc != 1) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "error printing flow: %s", strerror(errno));
    }

    g_string_free(rstr, TRUE);

    return (rc == 1);

}

#if 0
/**
 *yfPrintDelimited
 *
 *
 *
 */
gboolean yfPrintDelimited(
    FILE                *out,
    yfFlow_t            *flow,
    gboolean            yaft_mac,
    GError              **err)
{
    GString             *rstr = NULL;
    int                 rc = 0;

    rstr = g_string_new("");

    yfPrintDelimitedString(rstr, flow, yaft_mac);

    rc = fwrite(rstr->str, rstr->len, 1, out);

    if (rc != 1) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "error printing delimited flow: %s", strerror(errno));
    }

    g_string_free(rstr, TRUE);

    return (rc == 1);

}

/**
 * yfPrintColumnHeaders
 *
 *
 */
void yfPrintColumnHeaders(
    FILE           *out,
    gboolean       yaft_mac,
    GError         **err)
{

    GString        *rstr = NULL;

    rstr = g_string_new("");

    g_string_append_printf(rstr, "start-time%14s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "end-time%16s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "duration%s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "rtt%6s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "proto%s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "sip%36s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "sp%4s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "dip%38s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "dp%4s", YF_PRINT_DELIM);
    if (yaft_mac) {
        g_string_append_printf(rstr, "srcMacAddress%5s", YF_PRINT_DELIM);
        g_string_append_printf(rstr, "destMacAddress%4s", YF_PRINT_DELIM);
    }
    g_string_append_printf(rstr, "iflags%3s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "uflags%3s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "riflags%2s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "ruflags%2s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "isn%6s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "risn%5s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "tag%s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "rtag%s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "pkt%5s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "oct%6s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "rpkt%5s", YF_PRINT_DELIM);
    g_string_append_printf(rstr, "roct%5s", YF_PRINT_DELIM);

    g_string_append_printf(rstr, "end-reason");
    g_string_append(rstr,"\n");

    fwrite(rstr->str, rstr->len, 1, out);

    g_string_free(rstr, TRUE);

}
#endif
