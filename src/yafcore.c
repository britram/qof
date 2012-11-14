/**
 ** @internal
 ** yafcore.c
 ** YAF core I/O routines
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2006-2012 Carnegie Mellon University. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell, Chris Inacio, Emily Ecoff <ecoff@cert.org>
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
#include "yafctx.h"
#include <yaf/yafcore.h>
#include <yaf/decode.h>
#include <airframe/airutil.h>
#include <yaf/yafrag.h>

#define FBSTMLINIT(s, i, t) fbSubTemplateMultiListEntryInit(s, i, t, 1)
#define FBSTMLNEXT(p, s) fbSubTemplateMultiListGetNextEntry(p, s)

/** These are the template ID's for the templates that YAF uses to
    select the output. Template ID's are maintained for a set of
    basic flow types data
    * BASE which gets various additions added as the flow requires,
    * FULL base plus the internal fields are added
    * EXT (extended) which has the additional records in the
      yaf_extime_spec (extended time specification)

    WARNING: these need to be adjusted according to changes in the
    general & special dimensions */
#define YAF_FLOW_BASE_TID   0xB000 /* no general or special definitions */
#define YAF_FLOW_FULL_TID   0xB800 /* base no internal*/
#define YAF_FLOW_EXT_TID    0xB7FF /* everything except internal */

#define YAF_OPTIONS_TID     0xD000

/* 49154 - 49160 */
#define YAF_APP_FLOW_TID       0xC001 /* not used */
#define YAF_ENTROPY_FLOW_TID   0xC002
#define YAF_TCP_FLOW_TID       0xC003
#define YAF_MAC_FLOW_TID       0xC004
#define YAF_STATS_FLOW_TID     0xC005
#define YAF_P0F_FLOW_TID       0xC006
#define YAF_FPEXPORT_FLOW_TID  0xC007
#define YAF_PAYLOAD_FLOW_TID   0xC008

/** The dimensions are flags which determine which sets of fields will
    be exported out to an IPFIX record.  They are entries in a bitmap
    used to control the template. e.g. TCP flow information (seq num,
    tcp flags, etc.) only get added to the output record when the
    YTF_TCP flag is set; it only gets set when the transport protocol
    is set to 0x06. */

/** General dimensions */
#define YTF_BIF         0x0010
/* Special dimensions */
#define YTF_TOTAL       0x0001
#define YTF_DELTA       0x0002
#define YTF_TCP        0x0020
#define YTF_DAGIF       0x0040
#define YTF_FLE         0x0080
#define YTF_RLE         0x0100
#define YTF_IP4         0x0200
#define YTF_IP6         0x0400
#define YTF_INTERNAL    0x0800
#define YTF_ALL         0x0EFF /* this has to be everything _except_ RLE enabled */
#define YTF_REV         0xFF0F

/** If any of the FLE/RLE values are larger than this constant
    then we have to use FLE, otherwise, we choose RLE to
    conserve space/bandwidth etc.*/
#define YAF_RLEMAX      (1L << 31)

#define YF_PRINT_DELIM  "|"

/** include the CERT IE extensions for YAF */
#include "yaf/CERT_IE.h"

static uint64_t yaf_start_time = 0;

/* IPFIX definition of the full YAF flow record */
static fbInfoElementSpec_t yaf_flow_spec[] = {
    /* Millisecond start and end (epoch) (native time) */
    { "flowStartMilliseconds",              0, 0 },
    { "flowEndMilliseconds",                0, 0 },
    /* Counters */
    { "octetTotalCount",                    8, YTF_FLE | YTF_TOTAL },
    { "reverseOctetTotalCount",             8, YTF_FLE | YTF_TOTAL | YTF_BIF },
    { "packetTotalCount",                   8, YTF_FLE | YTF_TOTAL },
    { "reversePacketTotalCount",            8, YTF_FLE | YTF_TOTAL | YTF_BIF },
    /* delta Counters */
    { "octetDeltaCount",                    8, YTF_FLE | YTF_DELTA },
    { "reverseOctetDeltaCount",             8, YTF_FLE | YTF_DELTA | YTF_BIF },
    { "packetDeltaCount",                   8, YTF_FLE | YTF_DELTA },
    { "reversePacketDeltaCount",            8, YTF_FLE | YTF_DELTA | YTF_BIF },
    /* Reduced-length counters */
    { "octetTotalCount",                    4, YTF_RLE | YTF_TOTAL },
    { "reverseOctetTotalCount",             4, YTF_RLE | YTF_TOTAL| YTF_BIF },
    { "packetTotalCount",                   4, YTF_RLE | YTF_TOTAL },
    { "reversePacketTotalCount",            4, YTF_RLE | YTF_TOTAL | YTF_BIF },
    /* Reduced-length delta counters */
    { "octetDeltaCount",                    4, YTF_RLE | YTF_DELTA },
    { "reverseOctetDeltaCount",             4, YTF_RLE | YTF_DELTA | YTF_BIF },
    { "packetDeltaCount",                   4, YTF_RLE | YTF_DELTA },
    { "reversePacketDeltaCount",            4, YTF_RLE | YTF_DELTA | YTF_BIF },
    /* 5-tuple and flow status */
    { "sourceIPv6Address",                  0, YTF_IP6 },
    { "destinationIPv6Address",             0, YTF_IP6 },
    { "sourceIPv4Address",                  0, YTF_IP4 },
    { "destinationIPv4Address",             0, YTF_IP4 },
    { "sourceTransportPort",                0, 0 },
    { "destinationTransportPort",           0, 0 },
    { "flowAttributes",                     0, 0 },
    { "reverseFlowAttributes",              0, YTF_BIF },
    { "protocolIdentifier",                 0, 0 },
    { "flowEndReason",                      0, 0 },
    { "paddingOctets",                      2, YTF_INTERNAL },
    /* Round-trip time */
    { "reverseFlowDeltaMilliseconds",       0, YTF_BIF }, // 32-bit
    /*TCP Info would need to go here 4 SiLK & 4b padding*/
    { "tcpSequenceNumber",                  0, YTF_TCP },
    { "reverseTcpSequenceNumber",           0, YTF_TCP | YTF_BIF },
    { "initialTCPFlags",                    0, YTF_TCP },
    { "unionTCPFlags",                      0, YTF_TCP },
    { "reverseInitialTCPFlags",             0, YTF_TCP | YTF_BIF },
    { "reverseUnionTCPFlags",               0, YTF_TCP | YTF_BIF },
    { "vlanId",                             0, 0 },
    { "reverseVlanId",                      0, YTF_BIF },
    { "ingressInterface",                   0, YTF_DAGIF },
    { "egressInterface",                    0, YTF_DAGIF },
    { "subTemplateMultiList",               0, 0 },
    FB_IESPEC_NULL
};

static fbInfoElementSpec_t yaf_tcp_spec[] = {
    /* TCP-specific information */
    { "tcpSequenceNumber",                  0, 0 },
    { "initialTCPFlags",                    0, 0 },
    { "unionTCPFlags",                      0, 0 },
    { "reverseInitialTCPFlags",             0, YTF_BIF },
    { "reverseUnionTCPFlags",               0, YTF_BIF },
    { "reverseTcpSequenceNumber",           0, YTF_BIF },
    FB_IESPEC_NULL
};

/* MAC-specific information */
static fbInfoElementSpec_t yaf_mac_spec[] = {
    { "sourceMacAddress",                   0, 0 },
    { "destinationMacAddress",              0, 0 },
    FB_IESPEC_NULL
};

/* IPFIX definition of the YAF flow record time extension */
static fbInfoElementSpec_t yaf_extime_spec[] = {
    /* Microsecond start and end (RFC1305-style) (extended time) */
    { "flowStartMicroseconds",              0, 0 },
    { "flowEndMicroseconds",                0, 0 },
    /* Second start, end, and duration (extended time) */
    { "flowStartSeconds",                   0, 0 },
    { "flowEndSeconds",                     0, 0 },
    /* Flow durations (extended time) */
    { "flowDurationMicroseconds",           0, 0 },
    { "flowDurationMilliseconds",           0, 0 },
    /* Microsecond delta start and end (extended time) */
    { "flowStartDeltaMicroseconds",         0, 0 },
    { "flowEndDeltaMicroseconds",           0, 0 },
    FB_IESPEC_NULL
};

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
    { "meanFlowRate",                       0, 0 },
    { "meanPacketRate",                     0, 0 },
    FB_IESPEC_NULL
};


static fbInfoElementSpec_t yaf_flow_stats_spec[] = {
    { "dataByteCount",                      0, 0 },
    { "averageInterarrivalTime",            0, 0 },
    { "standardDeviationInterarrivalTime",  0, 0 },
    { "tcpUrgTotalCount",                   0, 0 },
    { "smallPacketCount",                   0, 0 },
    { "nonEmptyPacketCount",                0, 0 },
    { "largePacketCount",                   0, 0 },
    { "firstNonEmptyPacketSize",            0, 0 },
    { "maxPacketSize",                      0, 0 },
    { "standardDeviationPayloadLength",     0, 0 },
    { "firstEightNonEmptyPacketDirections", 0, 0 },
    { "paddingOctets",                      1, 1 },
    { "reverseDataByteCount",               0, YTF_BIF },
    { "reverseAverageInterarrivalTime",     0, YTF_BIF },
    { "reverseStandardDeviationInterarrivalTime", 0, YTF_BIF },
    { "reverseTcpUrgTotalCount",            0, YTF_BIF },
    { "reverseSmallPacketCount",            0, YTF_BIF },
    { "reverseNonEmptyPacketCount",         0, YTF_BIF },
    { "reverseLargePacketCount",            0, YTF_BIF },
    { "reverseFirstNonEmptyPacketSize",     0, YTF_BIF },
    { "reverseMaxPacketSize",               0, YTF_BIF },
    { "reverseStandardDeviationPayloadLength", 0, YTF_BIF },
    { "paddingOctets",                      2, 1 },
    FB_IESPEC_NULL
};

typedef struct yfFlowStatsRecord_st {
    uint64_t dataByteCount;
    uint64_t averageInterarrivalTime;
    uint64_t standardDeviationInterarrivalTime;
    uint32_t tcpUrgTotalCount;
    uint32_t smallPacketCount;
    uint32_t nonEmptyPacketCount;
    uint32_t largePacketCount;
    uint16_t firstNonEmptyPacketSize;
    uint16_t maxPacketSize;
    uint16_t standardDeviationPayloadLength;
    uint8_t  firstEightNonEmptyPacketDirections;
    uint8_t  padding[1];
    /* reverse Fields */
    uint64_t reverseDataByteCount;
    uint64_t reverseAverageInterarrivalTime;
    uint64_t reverseStandardDeviationInterarrivalTime;
    uint32_t reverseTcpUrgTotalCount;
    uint32_t reverseSmallPacketCount;
    uint32_t reverseNonEmptyPacketCount;
    uint32_t reverseLargePacketCount;
    uint16_t reverseFirstNonEmptyPacketSize;
    uint16_t reverseMaxPacketSize;
    uint16_t reverseStandardDeviationPayloadLength;
    uint8_t  padding2[2];
} yfFlowStatsRecord_t;

typedef struct yfTemplates_st {
    fbTemplate_t *ipfixStatsTemplate;
    fbTemplate_t *fstatsTemplate;
    fbTemplate_t *revfstatsTemplate;
    fbTemplate_t *tcpTemplate;
    fbTemplate_t *revTcpTemplate;
    fbTemplate_t *macTemplate;
} yfTemplates_t;

static yfTemplates_t yaf_tmpl;

/* IPv6-mapped IPv4 address prefix */
static uint8_t yaf_ip6map_pfx[12] =
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF };

/* Full YAF flow record. */
typedef struct yfIpfixFlow_st {
    uint64_t    flowStartMilliseconds;
    uint64_t    flowEndMilliseconds;
    uint64_t    octetTotalCount;
    uint64_t    reverseOctetTotalCount;
    uint64_t    packetTotalCount;
    uint64_t    reversePacketTotalCount;
    uint64_t    octetDeltaCount;
    uint64_t    reverseOctetDeltaCount;
    uint64_t    packetDeltaCount;
    uint64_t    reversePacketDeltaCount;
    uint8_t     sourceIPv6Address[16];
    uint8_t     destinationIPv6Address[16];
    uint32_t    sourceIPv4Address;
    uint32_t    destinationIPv4Address;
    uint16_t    sourceTransportPort;
    uint16_t    destinationTransportPort;
    uint16_t    flowAttributes;
    uint16_t    reverseFlowAttributes;
    uint8_t     protocolIdentifier;
    uint8_t     flowEndReason;
    uint8_t     paddingOctets[2];
    int32_t     reverseFlowDeltaMilliseconds;
    /* TCP specific info */
    uint32_t    tcpSequenceNumber;
    uint32_t    reverseTcpSequenceNumber;
    uint8_t     initialTCPFlags;
    uint8_t     unionTCPFlags;
    uint8_t     reverseInitialTCPFlags;
    uint8_t     reverseUnionTCPFlags;
    /* MAC Specific Info */
    uint16_t    vlanId;
    uint16_t    reverseVlanId;
    uint32_t    ingressInterface;
    uint32_t    egressInterface;
    fbSubTemplateMultiList_t subTemplateMultiList;
} yfIpfixFlow_t;

typedef struct yfTcpFlow_st {
    uint32_t    tcpSequenceNumber;
    uint8_t     initialTCPFlags;
    uint8_t     unionTCPFlags;
    uint8_t     reverseInitialTCPFlags;
    uint8_t     reverseUnionTCPFlags;
    uint32_t    reverseTcpSequenceNumber;
} yfTcpFlow_t;

typedef struct yfMacFlow_st {
    uint8_t     sourceMacAddress[6];
    uint8_t     destinationMacAddress[6];
} yfMacFlow_t;

typedef struct yfIpfixExtFlow_st {
    yfIpfixFlow_t   f;
    uint64_t    flowStartMicroseconds;
    uint64_t    flowEndMicroseconds;
    uint32_t    flowStartSeconds;
    uint32_t    flowEndSeconds;
    uint32_t    flowDurationMicroseconds;
    uint32_t    flowDurationMilliseconds;
    uint32_t    flowStartDeltaMicroseconds;
    uint32_t    flowEndDeltaMicroseconds;
} yfIpfixExtFlow_t;

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
    uint32_t    meanFlowRate;
    uint32_t    meanPacketRate;
} yfIpfixStats_t;

/* Core library configuration variables */
static gboolean yaf_core_export_payload = FALSE;
static gboolean yaf_core_map_ipv6 = FALSE;

/**
 * yfAlignmentCheck
 *
 * this checks the alignment of the template and corresponding record
 * ideally, all this magic would happen at compile time, but it
 * doesn't currently, (can't really do it in C,) so we do it at
 * run time
 *
 *
 * @param err a Glib error structure pointer initialized with an
 *        empty error on input, if an alignment error is detected
 *        then a new error will be put into the pointer.
 *
 */
void yfAlignmentCheck()
{
    size_t prevOffset = 0;
    size_t prevSize = 0;

#define DO_SIZE(S_,F_) (SIZE_T_CAST)sizeof(((S_ *)(0))->F_)
#define EA_STRING(S_,F_) "alignment error in struct " #S_ " for element "   \
                         #F_ " offset %#"SIZE_T_FORMATX" size %"            \
                         SIZE_T_FORMAT" (pad %"SIZE_T_FORMAT")",            \
                         (SIZE_T_CAST)offsetof(S_,F_), DO_SIZE(S_,F_),      \
                         (SIZE_T_CAST)(offsetof(S_,F_) % DO_SIZE(S_,F_))
#define EG_STRING(S_,F_) "gap error in struct " #S_ " for element " #F_     \
                         " offset %#"SIZE_T_FORMATX" size %"SIZE_T_FORMAT,  \
                         (SIZE_T_CAST)offsetof(S_,F_),                      \
                         DO_SIZE(S_,F_)
#define RUN_CHECKS(S_,F_,A_) {                                          \
        if (((offsetof(S_,F_) % DO_SIZE(S_,F_)) != 0) && A_) {          \
            g_error(EA_STRING(S_,F_));                                  \
        }                                                               \
        if (offsetof(S_,F_) != (prevOffset+prevSize)) {                 \
            g_error(EG_STRING(S_,F_));                                  \
            return;                                                     \
        }                                                               \
        prevOffset = offsetof(S_,F_);                                   \
        prevSize = DO_SIZE(S_,F_);                                      \
/*        fprintf(stderr, "%17s %40s %#5lx %3d %#5lx\n", #S_, #F_,      \
                offsetof(S_,F_), DO_SIZE(S_,F_),                        \
                offsetof(S_,F_)+DO_SIZE(S_,F_));*/                      \
    }

    RUN_CHECKS(yfIpfixFlow_t,flowStartMilliseconds,1);
    RUN_CHECKS(yfIpfixFlow_t,flowEndMilliseconds,1);
    RUN_CHECKS(yfIpfixFlow_t,octetTotalCount,1);
    RUN_CHECKS(yfIpfixFlow_t,reverseOctetTotalCount,1);
    RUN_CHECKS(yfIpfixFlow_t,packetTotalCount,1);
    RUN_CHECKS(yfIpfixFlow_t,reversePacketTotalCount,1);
    RUN_CHECKS(yfIpfixFlow_t,octetDeltaCount,1);
    RUN_CHECKS(yfIpfixFlow_t,reverseOctetDeltaCount,1);
    RUN_CHECKS(yfIpfixFlow_t,packetDeltaCount,1);
    RUN_CHECKS(yfIpfixFlow_t,reversePacketDeltaCount,1);
    RUN_CHECKS(yfIpfixFlow_t,sourceIPv6Address,1);
    RUN_CHECKS(yfIpfixFlow_t,destinationIPv6Address,1);
    RUN_CHECKS(yfIpfixFlow_t,sourceIPv4Address,1);
    RUN_CHECKS(yfIpfixFlow_t,destinationIPv4Address,1);
    RUN_CHECKS(yfIpfixFlow_t,sourceTransportPort,1);
    RUN_CHECKS(yfIpfixFlow_t,destinationTransportPort,1);
    RUN_CHECKS(yfIpfixFlow_t,flowAttributes,1);
    RUN_CHECKS(yfIpfixFlow_t,reverseFlowAttributes,1);
    RUN_CHECKS(yfIpfixFlow_t,protocolIdentifier,1);
    RUN_CHECKS(yfIpfixFlow_t,flowEndReason,1);
    RUN_CHECKS(yfIpfixFlow_t,paddingOctets,0);
    RUN_CHECKS(yfIpfixFlow_t,reverseFlowDeltaMilliseconds,1);

    /* TCP stuff for SiLK only! */
    RUN_CHECKS(yfIpfixFlow_t,tcpSequenceNumber,1);
    RUN_CHECKS(yfIpfixFlow_t,reverseTcpSequenceNumber,1);
    RUN_CHECKS(yfIpfixFlow_t,initialTCPFlags,1);
    RUN_CHECKS(yfIpfixFlow_t,unionTCPFlags,1);
    RUN_CHECKS(yfIpfixFlow_t,reverseInitialTCPFlags,1);
    RUN_CHECKS(yfIpfixFlow_t,reverseUnionTCPFlags,1);

    RUN_CHECKS(yfIpfixFlow_t,vlanId,1);
    RUN_CHECKS(yfIpfixFlow_t,reverseVlanId,1);

    RUN_CHECKS(yfIpfixFlow_t,ingressInterface,1);
    RUN_CHECKS(yfIpfixFlow_t,egressInterface,1);

    RUN_CHECKS(yfIpfixFlow_t,subTemplateMultiList,0);
    prevOffset = 0;
    prevSize = 0;

    RUN_CHECKS(yfIpfixExtFlow_t,f,1);
    RUN_CHECKS(yfIpfixExtFlow_t,flowStartMicroseconds,1);
    RUN_CHECKS(yfIpfixExtFlow_t,flowEndMicroseconds,1);
    RUN_CHECKS(yfIpfixExtFlow_t,flowStartSeconds,1);
    RUN_CHECKS(yfIpfixExtFlow_t,flowEndSeconds,1);
    RUN_CHECKS(yfIpfixExtFlow_t,flowDurationMicroseconds,1);
    RUN_CHECKS(yfIpfixExtFlow_t,flowDurationMilliseconds,1);
    RUN_CHECKS(yfIpfixExtFlow_t,flowStartDeltaMicroseconds,1);
    RUN_CHECKS(yfIpfixExtFlow_t,flowEndDeltaMicroseconds,1);
    prevOffset = 0;
    prevSize = 0;

    RUN_CHECKS(yfIpfixStats_t, systemInitTimeMilliseconds,1);
    RUN_CHECKS(yfIpfixStats_t, exportedFlowTotalCount,1);
    RUN_CHECKS(yfIpfixStats_t, packetTotalCount, 1);
    RUN_CHECKS(yfIpfixStats_t, droppedPacketTotalCount,1);
    RUN_CHECKS(yfIpfixStats_t, ignoredPacketTotalCount, 1);
    RUN_CHECKS(yfIpfixStats_t, notSentPacketTotalCount, 1);
    RUN_CHECKS(yfIpfixStats_t, expiredFragmentCount,1);
    RUN_CHECKS(yfIpfixStats_t, assembledFragmentCount,1);
    RUN_CHECKS(yfIpfixStats_t, flowTableFlushEvents,1);
    RUN_CHECKS(yfIpfixStats_t, flowTablePeakCount,1);
    RUN_CHECKS(yfIpfixStats_t, exporterIPv4Address,1);
    RUN_CHECKS(yfIpfixStats_t, exportingProcessId, 1);
    RUN_CHECKS(yfIpfixStats_t, meanFlowRate, 1);
    RUN_CHECKS(yfIpfixStats_t, meanPacketRate, 1);

    prevOffset = 0;
    prevSize = 0;

    RUN_CHECKS(yfFlowStatsRecord_t, dataByteCount, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, averageInterarrivalTime, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, standardDeviationInterarrivalTime, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, tcpUrgTotalCount, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, smallPacketCount, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, nonEmptyPacketCount, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, largePacketCount, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, firstNonEmptyPacketSize, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, maxPacketSize, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, standardDeviationPayloadLength, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, firstEightNonEmptyPacketDirections, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, padding, 0);
    RUN_CHECKS(yfFlowStatsRecord_t, reverseDataByteCount, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, reverseAverageInterarrivalTime, 1);
    RUN_CHECKS(yfFlowStatsRecord_t,reverseStandardDeviationInterarrivalTime,1);
    RUN_CHECKS(yfFlowStatsRecord_t, reverseTcpUrgTotalCount, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, reverseSmallPacketCount, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, reverseNonEmptyPacketCount, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, reverseLargePacketCount, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, reverseFirstNonEmptyPacketSize, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, reverseMaxPacketSize, 1);
    RUN_CHECKS(yfFlowStatsRecord_t, reverseStandardDeviationPayloadLength,1);
    RUN_CHECKS(yfFlowStatsRecord_t, padding2, 0);

#undef DO_SIZE
#undef EA_STRING
#undef EG_STRING
#undef RUN_CHECKS

}

void yfWriterExportPayload(
    gboolean            payload_mode)
{
    yaf_core_export_payload = payload_mode;
}

void yfWriterExportMappedV6(
    gboolean            map_mode)
{
    yaf_core_map_ipv6 = map_mode;
}


/**
 * yfFlowPrepare
 *
 * initialize the state of a flow to be "clean" so that they
 * can be reused
 *
 */
void yfFlowPrepare(
    yfFlow_t          *flow)
{

    memset(flow->sourceMacAddr, 0, ETHERNET_MAC_ADDR_LENGTH);
    memset(flow->destinationMacAddr, 0, ETHERNET_MAC_ADDR_LENGTH);

}


/**
 *yfFlowCleanup
 *
 * cleans up after a flow is no longer needed by deallocating
 * the dynamic memory allocated to the flow (think payload)
 *
 */
void yfFlowCleanup(
    yfFlow_t          *flow)
{
    /* this is a nop - removed after payload removal. 
       stick dynalloc tcp stuff here? */
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
        fbInfoModelAddElementArray(yaf_model, yaf_info_elements);

    }

    return yaf_model;
}

/**
 * yfInitExporterSession
 *
 *
 */
static fbSession_t *yfInitExporterSession(
    uint32_t        domain,
    GError          **err)
{
    fbInfoModel_t   *model = yfInfoModel();
    fbTemplate_t    *tmpl = NULL;
    fbSession_t     *session = NULL;
    time_t           cur_time = time(NULL);

    yaf_start_time = cur_time * 1000;

    /* Allocate the session */
    session = fbSessionAlloc(model);

   /* set observation domain */
    fbSessionSetDomain(session, domain);

   /* Create the full record template */
    tmpl = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(tmpl, yaf_flow_spec, YTF_ALL, err))
        return NULL;

    /* Add the full record template to the session */
    if (!fbSessionAddTemplate(session, TRUE, YAF_FLOW_FULL_TID, tmpl, err)) {
        return NULL;
    }
    if (!fbSessionAddTemplate(session, FALSE, YAF_FLOW_FULL_TID, tmpl, err)) {
        return NULL;
    }

    /* Create the Options Template */
    yaf_tmpl.ipfixStatsTemplate = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(yaf_tmpl.ipfixStatsTemplate,
                                   yaf_stats_option_spec, 0, err))
    {
        return NULL;
    }

    /* Scope fields are exporterIPv4Address and exportingProcessID */
    fbTemplateSetOptionsScope(yaf_tmpl.ipfixStatsTemplate, 2);
    if (!fbSessionAddTemplate(session, TRUE, YAF_OPTIONS_TID,
                              yaf_tmpl.ipfixStatsTemplate, err))
    {
        return NULL;
    }

    if (!fbSessionAddTemplate(session, FALSE, YAF_OPTIONS_TID,
                              yaf_tmpl.ipfixStatsTemplate, err))
    {
        return NULL;
    }

    /* Flow Stats Template */
    yaf_tmpl.fstatsTemplate = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(yaf_tmpl.fstatsTemplate,
                                   yaf_flow_stats_spec, 0, err))
    {
        return NULL;
    }
    if (!fbSessionAddTemplate(session, FALSE, YAF_STATS_FLOW_TID,
                              yaf_tmpl.fstatsTemplate, err))
    {
        return NULL;
    }

    yaf_tmpl.revfstatsTemplate = fbTemplateAlloc(model);

    if (!fbTemplateAppendSpecArray(yaf_tmpl.revfstatsTemplate,
                                   yaf_flow_stats_spec, 0xffffffff, err))
    {
        return NULL;
    }
    if (!fbSessionAddTemplate(session, FALSE, YAF_STATS_FLOW_TID | YTF_BIF,
                              yaf_tmpl.revfstatsTemplate, err))
    {
        return NULL;
    }
    if (!fbSessionAddTemplate(session, TRUE, YAF_STATS_FLOW_TID,
                              yaf_tmpl.revfstatsTemplate, err))
    {
        return NULL;
    }

    yaf_tmpl.tcpTemplate = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(yaf_tmpl.tcpTemplate, yaf_tcp_spec, 0, err))
    {
        return NULL;
    }
    if (!fbSessionAddTemplate(session, FALSE, YAF_TCP_FLOW_TID, yaf_tmpl.tcpTemplate, err)) {
        return NULL;
    }
    yaf_tmpl.revTcpTemplate = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray( yaf_tmpl.revTcpTemplate, yaf_tcp_spec, 0xffffffff, err)) {
        return NULL;
    }
    if (!fbSessionAddTemplate(session, FALSE, YAF_TCP_FLOW_TID | YTF_BIF, yaf_tmpl.revTcpTemplate, err)) {
        return NULL;
    }

    if (!fbSessionAddTemplate(session, TRUE, YAF_TCP_FLOW_TID, yaf_tmpl.revTcpTemplate, err)) {
        return NULL;
    }

    yaf_tmpl.macTemplate = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(yaf_tmpl.macTemplate, yaf_mac_spec, 0xffffffff, err)) {
        return NULL;
    }
    if (!fbSessionAddTemplate(session, FALSE, YAF_MAC_FLOW_TID, yaf_tmpl.macTemplate, err)) {
        return NULL;
    }
    if (!fbSessionAddTemplate(session, TRUE, YAF_MAC_FLOW_TID, yaf_tmpl.macTemplate, err)) {
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
    tmpl = fbTemplateAlloc(yfInfoModel());

    if (!fbTemplateAppendSpecArray(tmpl, yaf_flow_spec,
                                   (tid & (~YAF_FLOW_BASE_TID)), err))    {
        return FALSE;
    }

    if (!fbSessionAddTemplate(session, FALSE, tid, tmpl, err)) {
        return FALSE;
    }

    /* Template should be loaded. Try setting the template again. */
    return fBufSetExportTemplate(fbuf, tid, err);
}

/**
 *yfWriteStatsFlow
 *
 *
 */
gboolean yfWriteStatsFlow(
    void *yfContext,
    uint32_t pcap_drop,
    GTimer *timer,
    GError **err)
{
    yfIpfixStats_t      rec;
    yfContext_t         *ctx = (yfContext_t *)yfContext;
    fBuf_t              *fbuf = ctx->fbuf;
    uint32_t            mask = 0x000000FF;
    char                buf[200];
    static struct hostent *host;
    static uint32_t     host_ip = 0;

    yfGetFlowTabStats(ctx->flowtab, &(rec.packetTotalCount),
                      &(rec.exportedFlowTotalCount),
                      &(rec.notSentPacketTotalCount),
                      &(rec.flowTablePeakCount), &(rec.flowTableFlushEvents));
    if (ctx->fragtab) {
        yfGetFragTabStats(ctx->fragtab, &(rec.expiredFragmentCount),
                          &(rec.assembledFragmentCount));
    } else {
        rec.expiredFragmentCount = 0;
        rec.assembledFragmentCount = 0;
    }

    if (!fbuf) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_IO,
                    "Error Writing Stats Message: No fbuf [output] Available");
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
    rec.ignoredPacketTotalCount = yfGetDecodeStats(ctx->dectx);

    /* Dropped packets - from yafcap.c & libpcap */
    rec.droppedPacketTotalCount = pcap_drop;
    rec.exporterIPv4Address = host_ip;

    /* Use Observation ID as exporting Process ID */
    rec.exportingProcessId = ctx->cfg->odid;

    rec.meanFlowRate = rec.exportedFlowTotalCount/g_timer_elapsed(timer, NULL);
    rec.meanPacketRate = rec.packetTotalCount / g_timer_elapsed(timer, NULL);

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
    void                *yfContext,
    yfFlow_t            *flow,
    GError              **err)
{
    yfIpfixFlow_t       rec;
    uint16_t            wtid;
    uint16_t            etid = 0; /* extra templates */
    fbSubTemplateMultiListEntry_t *stml = NULL;
    yfTcpFlow_t         *tcprec = NULL;
    yfMacFlow_t         *macrec = NULL;
    int                 tmplcount = 0;
    gboolean            ok;
    int32_t             temp = 0;
    int                 loop, count;
    yfContext_t         *ctx = (yfContext_t *)yfContext;
    fBuf_t              *fbuf = ctx->fbuf;
    yfFlowStatsRecord_t *statsflow = NULL;

    /* copy time */
    rec.flowStartMilliseconds = flow->stime;
    rec.flowEndMilliseconds = flow->etime;
    rec.reverseFlowDeltaMilliseconds = flow->rdtime;

    /* copy addresses */
    if (yaf_core_map_ipv6 && (flow->key.version == 4)) {
        memcpy(rec.sourceIPv6Address, yaf_ip6map_pfx,
               sizeof(yaf_ip6map_pfx));
        *(uint32_t *)(&(rec.sourceIPv6Address[sizeof(yaf_ip6map_pfx)])) =
            g_htonl(flow->key.addr.v4.sip);
        memcpy(rec.destinationIPv6Address, yaf_ip6map_pfx,
               sizeof(yaf_ip6map_pfx));
        *(uint32_t *)(&(rec.destinationIPv6Address[sizeof(yaf_ip6map_pfx)])) =
            g_htonl(flow->key.addr.v4.dip);
    } else if (flow->key.version == 4) {
        rec.sourceIPv4Address = flow->key.addr.v4.sip;
        rec.destinationIPv4Address = flow->key.addr.v4.dip;
    } else if (flow->key.version == 6) {
        memcpy(rec.sourceIPv6Address, flow->key.addr.v6.sip,
               sizeof(rec.sourceIPv6Address));
        memcpy(rec.destinationIPv6Address, flow->key.addr.v6.dip,
               sizeof(rec.destinationIPv6Address));
    } else {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                    "Illegal IP version %u", flow->key.version);
    }


    /* choose options for basic template */
    wtid = YAF_FLOW_BASE_TID;

    rec.vlanId = flow->key.vlanId;
    /* right? */
    rec.reverseVlanId = flow->key.vlanId;

    /* copy key and counters */
    rec.sourceTransportPort = flow->key.sp;
    rec.destinationTransportPort = flow->key.dp;
    rec.flowAttributes = flow->val.attributes;
    rec.reverseFlowAttributes = flow->rval.attributes;
    rec.protocolIdentifier = flow->key.proto;
    rec.flowEndReason = flow->reason;

    if (ctx->cfg->deltamode) {
        rec.octetDeltaCount = flow->val.oct;
        rec.reverseOctetDeltaCount = flow->rval.oct;
        rec.packetDeltaCount = flow->val.pkt;
        rec.reversePacketDeltaCount = flow->rval.pkt;
        wtid |= YTF_DELTA;
    } else {
        rec.octetTotalCount = flow->val.oct;
        rec.reverseOctetTotalCount = flow->rval.oct;
        rec.packetTotalCount = flow->val.pkt;
        rec.reversePacketTotalCount = flow->rval.pkt;
        wtid |= YTF_TOTAL;
    }

    rec.ingressInterface = ctx->cfg->ingressInt;
    rec.egressInterface = ctx->cfg->egressInt;

#if YAF_ENABLE_DAG_SEPARATE_INTERFACES || YAF_ENABLE_NAPATECH_SEPARATE_INTERFACES
    rec.ingressInterface = flow->key.netIf;
    rec.egressInterface  = flow->key.netIf | 0x100;
#endif

#if YAF_ENABLE_BIVIO
    rec.ingressInterface = flow->val.netIf;
    if (rec.reversePacketTotalCount) {
        rec.egressInterface = flow->rval.netIf;
    } else {
        rec.egressInterface = flow->val.netIf | 0x100;
    }
#endif

    if (rec.reversePacketTotalCount) {
        wtid |= YTF_BIF;
        etid = YTF_BIF;
    }

    if (rec.protocolIdentifier == YF_PROTO_TCP) {
        rec.tcpSequenceNumber = flow->val.isn;
        rec.reverseTcpSequenceNumber = flow->rval.isn;
        rec.initialTCPFlags = flow->val.iflags;
        rec.reverseInitialTCPFlags = flow->rval.iflags;
        rec.unionTCPFlags = flow->val.uflags;
        rec.reverseUnionTCPFlags = flow->rval.uflags;
        wtid |= YTF_TCP;
    }

    if (rec.octetTotalCount < YAF_RLEMAX &&
        rec.reverseOctetTotalCount < YAF_RLEMAX &&
        rec.packetTotalCount < YAF_RLEMAX &&
        rec.reversePacketTotalCount < YAF_RLEMAX) {
        wtid |= YTF_RLE;
    } else {
        wtid |= YTF_FLE;
    }

    if (yaf_core_map_ipv6 || (flow->key.version == 6)) {
        wtid |= YTF_IP6;
    } else {
        wtid |= YTF_IP4;
    }

    if (rec.ingressInterface || rec.egressInterface) {
        wtid |= YTF_DAGIF;
    }

#if YAF_ENABLE_DAG_SEPARATE_INTERFACES
    if (ctx->cfg->dagInterface) {
        wtid |= YTF_DAGIF;
    }
#endif

#if YAF_ENABLE_BIVIO
    wtid |= YTF_DAGIF
#endif

#if YAF_ENABLE_NAPATECH_SEPARATE_INTERFACES
    if (ctx->cfg->pcapxInterface) {
        wtid |= YTF_DAGIF;
    }
#endif

    if (!yfSetExportTemplate(fbuf, wtid, err)) {
        return FALSE;
    }

    if (ctx->cfg->macmode) {
        tmplcount++;
    }

    if (ctx->cfg->statsmode) {
        if (flow->val.stats.payoct || flow->rval.stats.payoct ||
            (flow->val.stats.aitime > flow->val.pkt) ||
            (flow->rval.stats.aitime > flow->rval.pkt))
        {
            tmplcount++;
        }
    }

    /* Initialize SubTemplateMultiList with number of templates we are to add*/
    fbSubTemplateMultiListInit(&(rec.subTemplateMultiList), 0, tmplcount);
    fbSubTemplateMultiListSetSemantic(&(rec.subTemplateMultiList), 0);

    /* Add TCP Template - IF TCP Flow and SiLK Mode is OFF */
    if (flow->key.proto == YF_PROTO_TCP && !ctx->cfg->silkmode) {
        stml = FBSTMLNEXT(&(rec.subTemplateMultiList), stml);
        if (etid) {
            tcprec = (yfTcpFlow_t *)FBSTMLINIT(stml,
                                               (YAF_TCP_FLOW_TID | etid),
                                               yaf_tmpl.revTcpTemplate);
            tcprec->reverseTcpSequenceNumber = flow->rval.isn;
            tcprec->reverseInitialTCPFlags = flow->rval.iflags;
            tcprec->reverseUnionTCPFlags = flow->rval.uflags;
        } else {
            tcprec = (yfTcpFlow_t *)FBSTMLINIT(stml, YAF_TCP_FLOW_TID,
                                               yaf_tmpl.tcpTemplate);
        }
        tcprec->tcpSequenceNumber = flow->val.isn;
        tcprec->initialTCPFlags = flow->val.iflags;
        tcprec->unionTCPFlags = flow->val.uflags;
        tmplcount--;
    }

    /* Add MAC Addresses */
    if (ctx->cfg->macmode) {
        stml = FBSTMLNEXT(&(rec.subTemplateMultiList), stml);
        macrec = (yfMacFlow_t *)FBSTMLINIT(stml, YAF_MAC_FLOW_TID,
                                           yaf_tmpl.macTemplate);
        memcpy(macrec->sourceMacAddress, flow->sourceMacAddr,
               ETHERNET_MAC_ADDR_LENGTH);
        memcpy(macrec->destinationMacAddress, flow->destinationMacAddr,
               ETHERNET_MAC_ADDR_LENGTH);
        tmplcount--;
    }

    if (ctx->cfg->statsmode && (flow->val.stats.payoct ||
                                flow->rval.stats.payoct ||
                                (flow->val.stats.aitime > flow->val.pkt) ||
                                (flow->rval.stats.aitime > flow->rval.pkt)))
    {
        uint16_t pktavg;
        stml = FBSTMLNEXT(&(rec.subTemplateMultiList), stml);
        if (etid) {
            statsflow =
                (yfFlowStatsRecord_t *)FBSTMLINIT(stml,
                                                  (YAF_STATS_FLOW_TID | etid),
                                                  yaf_tmpl.revfstatsTemplate);
            statsflow->reverseTcpUrgTotalCount = flow->rval.stats.tcpurgct;
            statsflow->reverseSmallPacketCount = flow->rval.stats.smallpktct;
            statsflow->reverseFirstNonEmptyPacketSize =
                flow->rval.stats.firstpktsize;
            statsflow->reverseNonEmptyPacketCount =
                flow->rval.stats.nonemptypktct;
            statsflow->reverseLargePacketCount =
                flow->rval.stats.largepktct;
            statsflow->reverseDataByteCount = flow->rval.stats.payoct;
            count = (statsflow->reverseNonEmptyPacketCount > 10) ? 10 : statsflow->reverseNonEmptyPacketCount;
            pktavg = flow->rval.stats.payoct / flow->rval.stats.nonemptypktct;
            for (loop = 0; loop < count; loop++) {
                temp += (pow(abs(flow->rval.stats.pktsize[loop] - pktavg), 2));
            }
            if (count) {
                statsflow->reverseStandardDeviationPayloadLength =
                    sqrt(temp / count);
            }
            if (flow->rval.pkt > 1) {
                uint64_t time_temp = 0;
                statsflow->reverseAverageInterarrivalTime =
                    flow->rval.stats.aitime /(flow->rval.pkt - 1);
                count = (flow->rval.pkt > 11) ? 10 : (flow->rval.pkt - 1);
                for (loop = 0; loop < count; loop++) {
                    time_temp += (pow(labs(flow->rval.stats.iaarray[loop] -
                                          statsflow->reverseAverageInterarrivalTime), 2));
                }
                statsflow->reverseStandardDeviationInterarrivalTime =
                    sqrt(time_temp / count);
            }
            statsflow->reverseMaxPacketSize = flow->rval.stats.maxpktsize;
        } else {
            statsflow = (yfFlowStatsRecord_t *)FBSTMLINIT(stml,
                                                          YAF_STATS_FLOW_TID,
                                                      yaf_tmpl.fstatsTemplate);
        }

        statsflow->tcpUrgTotalCount = flow->val.stats.tcpurgct;
        statsflow->smallPacketCount = flow->val.stats.smallpktct;
        statsflow->firstNonEmptyPacketSize = flow->val.stats.firstpktsize;
        statsflow->nonEmptyPacketCount = flow->val.stats.nonemptypktct;
        statsflow->dataByteCount = flow->val.stats.payoct;
        statsflow->maxPacketSize = flow->val.stats.maxpktsize;
        statsflow->firstEightNonEmptyPacketDirections = flow->pktdir;
        statsflow->largePacketCount = flow->val.stats.largepktct;
        temp = 0;
        count = (statsflow->nonEmptyPacketCount < 10) ? statsflow->nonEmptyPacketCount : 10;
        pktavg = flow->val.stats.payoct / flow->val.stats.nonemptypktct;
        for (loop = 0; loop < count; loop++) {
            temp += (pow(abs(flow->val.stats.pktsize[loop] - pktavg), 2));
        }
        if (count) {
            statsflow->standardDeviationPayloadLength =
                sqrt(temp / count);
        }
        if (flow->val.pkt > 1) {
            uint64_t time_temp = 0;
            statsflow->averageInterarrivalTime = flow->val.stats.aitime /
                                                 (flow->val.pkt - 1);
            count = (flow->val.pkt > 11) ? 10 : (flow->val.pkt - 1);
            for (loop = 0; loop < count; loop++) {
                time_temp += (pow(labs(flow->val.stats.iaarray[loop] -
                                       statsflow->averageInterarrivalTime),2));
            }
            statsflow->standardDeviationInterarrivalTime=sqrt(time_temp/count);
        }
        tmplcount--;
    }

    /* IF UDP - Check to see if we need to re-export templates */
    /* We do not advise in using UDP (nicer than saying you're stupid) */
    if ((ctx->cfg->connspec.transport == FB_UDP) ||
        (ctx->cfg->connspec.transport == FB_DTLS_UDP))
    {
        /* 3 is the factor from RFC 5101 as a recommendation of how often
           between timeouts to resend */
        if ((flow->etime > ctx->lastUdpTempTime) &&
            ((flow->etime - ctx->lastUdpTempTime) >
             ((ctx->cfg->yaf_udp_template_timeout)/3)))
        {
            /* resend templates */
            ok = fbSessionExportTemplates(fBufGetSession(ctx->fbuf), err);
            ctx->lastUdpTempTime = flow->etime;
            if (!ok) {
                g_warning("Failed to renew UDP Templates: %s",
                          (*err)->message);
                g_clear_error(err);
            }
        }
    }

    /* Now append the record to the buffer */
    if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
        return FALSE;
    }

    /* Clear MultiList */
    fbSubTemplateMultiListClear(&(rec.subTemplateMultiList));

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
    tmpl = fbTemplateAlloc(model);

    if (!fbTemplateAppendSpecArray(tmpl, yaf_flow_spec, YTF_ALL, err))
        return NULL;
    if (!fbSessionAddTemplate(session, TRUE, YAF_FLOW_FULL_TID, tmpl, err))
        return NULL;

    yaf_tmpl.tcpTemplate = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(yaf_tmpl.tcpTemplate, yaf_tcp_spec,
                                   0xffffffff, err))
    {
        return NULL;
    }
    if (!fbSessionAddTemplate(session, TRUE, YAF_TCP_FLOW_TID,
                              yaf_tmpl.tcpTemplate, err))
    {
        return NULL;
    }

    yaf_tmpl.macTemplate = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(yaf_tmpl.macTemplate, yaf_mac_spec,
                                   0xffffffff, err))
    {
        return NULL;
    }
    if (!fbSessionAddTemplate(session, TRUE, YAF_MAC_FLOW_TID,
                              yaf_tmpl.macTemplate, err))
    {
        return NULL;
    }

    /* Add the extended record template */
    tmpl = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(tmpl, yaf_flow_spec, YTF_ALL, err))
        return NULL;
    if (!fbTemplateAppendSpecArray(tmpl, yaf_extime_spec, YTF_ALL, err))
        return NULL;
    if (!fbSessionAddTemplate(session, TRUE, YAF_FLOW_EXT_TID, tmpl, err))
        return NULL;

    /* Done. Return the session. */

    /** Add the template callback so we don't try to decode DPI */
    fbSessionAddTemplateCallback(session, yfTemplateCallback);

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
gboolean yfReadFlow(
    fBuf_t          *fbuf,
    yfFlow_t        *flow,
    GError          **err)
{
    yfIpfixFlow_t       rec;
    size_t              len;
    fbSubTemplateMultiListEntry_t *stml = NULL;
    yfTcpFlow_t         *tcprec = NULL;
    fbTemplate_t        *next_tmpl = NULL;
    yfMacFlow_t         *macrec = NULL;

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
    flow->val.oct = rec.octetTotalCount;
    flow->val.pkt = rec.packetTotalCount;
    if (flow->val.oct == 0 && flow->val.pkt == 0) {
        flow->val.oct = rec.octetDeltaCount;
        flow->val.pkt = rec.packetDeltaCount;
    }
    flow->key.vlanId = rec.vlanId;
    flow->rval.oct = rec.reverseOctetTotalCount;
    flow->rval.pkt = rec.reversePacketTotalCount;
    flow->reason = rec.flowEndReason;

    flow->val.isn = rec.tcpSequenceNumber;
    flow->val.iflags = rec.initialTCPFlags;
    flow->val.uflags = rec.unionTCPFlags;
    flow->rval.isn = rec.reverseTcpSequenceNumber;
    flow->rval.iflags = rec.reverseInitialTCPFlags;
    flow->rval.uflags = rec.reverseUnionTCPFlags;

    /* Get subTemplateMultiList Entry */
    while ((stml = FBSTMLNEXT(&(rec.subTemplateMultiList), stml)))
    {
        switch ((stml->tmplID & YTF_REV)) {

        case YAF_TCP_FLOW_TID:
            tcprec = (yfTcpFlow_t *)fbSubTemplateMultiListEntryNextDataPtr(stml, tcprec);
            flow->val.isn = tcprec->tcpSequenceNumber;
            flow->val.iflags = tcprec->initialTCPFlags;
            flow->val.uflags = tcprec->unionTCPFlags;
            if ((stml->tmplID & YTF_BIF)) {
                flow->rval.isn = tcprec->reverseTcpSequenceNumber;
                flow->rval.iflags = tcprec->reverseInitialTCPFlags;
                flow->rval.uflags = tcprec->reverseUnionTCPFlags;
            }
            break;
        case YAF_MAC_FLOW_TID:
            macrec = (yfMacFlow_t *)fbSubTemplateMultiListEntryNextDataPtr(stml, macrec);
            memcpy(flow->sourceMacAddr, macrec->sourceMacAddress,
                   ETHERNET_MAC_ADDR_LENGTH);
            memcpy(flow->destinationMacAddr, macrec->destinationMacAddress,
                   ETHERNET_MAC_ADDR_LENGTH);
            break;
        default:
            /* don't know about this template */
            break;
        }
    }

    fbSubTemplateMultiListClear(&(rec.subTemplateMultiList));

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
    fbSubTemplateMultiListEntry_t *stml = NULL;
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

    while ((stml = FBSTMLNEXT(&(rec.f.subTemplateMultiList), stml)))
    {
        switch ((stml->tmplID & YTF_REV)) {
        case YAF_TCP_FLOW_TID:
            tcprec = (yfTcpFlow_t *)fbSubTemplateMultiListEntryNextDataPtr(stml, tcprec);
            flow->val.isn = tcprec->tcpSequenceNumber;
            flow->val.iflags = tcprec->initialTCPFlags;
            flow->val.uflags = tcprec->unionTCPFlags;
            if ((stml->tmplID & YTF_BIF)) {
                flow->rval.isn = tcprec->reverseTcpSequenceNumber;
                flow->rval.iflags = tcprec->reverseInitialTCPFlags;
                flow->rval.uflags = tcprec->reverseUnionTCPFlags;
            }
            break;
        case YAF_MAC_FLOW_TID:
            macrec = (yfMacFlow_t *)fbSubTemplateMultiListEntryNextDataPtr(stml, macrec);
            memcpy(flow->sourceMacAddr, macrec->sourceMacAddress,
                   ETHERNET_MAC_ADDR_LENGTH);
            memcpy(flow->destinationMacAddr, macrec->destinationMacAddress,
                   ETHERNET_MAC_ADDR_LENGTH);
            break;
        default:
            fbSubTemplateMultiListEntryNextDataPtr(stml, NULL);
            break;
        }
    }

    fbSubTemplateMultiListClear(&(rec.f.subTemplateMultiList));

    return TRUE;
}

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
            g_string_append_printf(rstr, " tcp %s:%u => %s:%u %08x:%08x ",
                                   sabuf, flow->key.sp, dabuf, flow->key.dp,
                                   flow->val.isn, flow->rval.isn);
        } else {
            g_string_append_printf(rstr, " tcp %s:%u => %s:%u %08x ",
                                   sabuf, flow->key.sp, dabuf, flow->key.dp,
                                   flow->val.isn);
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
