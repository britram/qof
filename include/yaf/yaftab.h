/*
 ** yaftab.h
 ** YAF Active Flow Table
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2006-2012 Carnegie Mellon University. All Rights Reserved.
 ** Copyright (C) 2012-2013 Brian Trammell.             All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell <brian@trammell.ch>
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

/*
 * This is the documentation for the _old_ yaftab.h; it is no longer current,
 * and should not be read by anyone.
 *
 * Flow generation interface for YAF. This facility works by maintaining a
 * current flow table. Packets may be added to the active flows within this
 * table using the yfFlowPkt() call. Completed flows may be written to an
 * IPFIX message buffer using yfFlowFlush().
 *
 * The flow table is configured by a number of global variables.
 *
 * <tt>yaf_idle</tt> sets
 * the idle timeout in seconds. A flow that receives no packets for the idle
 * timeout is assumed to be complete. The idle timeout is set to 300 seconds
 * (five minutes) by default.
 *
 * <tt>yaf_active</tt> sets the active timeout in seconds.
 * The maximum duration of a flow is the active timeout; additional packets
 * for the same flow will be counted as part of a new flow. The active timeout
 * is set to 1800 seconds (half an hour) by default.
 *
 * <tt>yaf_flowlim</tt> sets the maximum size of the flow table; flows exceeding
 * this limit will be expired in least-recent order, as if they were idle. The
 * flow limit defaults to zero, for no limit. Use this global to limit resource
 * usage by the flow table.
 *
 * <tt>yaf_paylen</tt> sets the number of bytes of payload to capture from the
 * start of each flow. The payload length defaults to zero, which disables
 * payload capture.
 *
 * <tt>yaf_uniflow</tt>, if TRUE, exports flows in uniflow mode, using the
 * record adjacency export method described in section 3 of
 * draft-ietf-ipfix-biflow. Defaults to FALSE.
 *
 * <tt>yaf_macmode</tt>, if TRUE, exports layer 2 information with each flow;
 * presently this is limited to VLAN tags but may be expanded to include the
 * MPLS stack and MAC addresses in the future. Defaults to FALSE.
 *
 * <tt>yaf_silkmode</tt>, if TRUE, enables SiLK compatibility mode. In this
 * mode, totalOctetCount and reverseTotalOctetCount are clamped to 32 bits.
 * Any packet that would cause either of these counters to overflow 32 bits
 * will force an active timeout. The high-order bit of the flowEndReason IE
 * is set on any flow created on a counter overflow, as above, or on an active
 * timeout. Defaults to FALSE.
 *
 * <tt>yaf_reqtype</tt> limits the flow table to collecting IPv4 or IPv6 flows
 * only. Set to YF_TYPE_IPv4 for IPv4 flows only, YF_TYPE_IPv6 for IPv6 flows
 * only, or YF_TYPE_IPANY (the default) to collect both IPv4 and IPv6 flows.
 *
 * This facility is used by YAF to assemble packets into flows.
 */

/**
 * @file
 *
 * Flow generation interface for YAF. [TODO - frontmatter]
 *
 * This facility is used by YAF to assemble packets into flows.
 */

#ifndef _YAF_TAB_H_
#define _YAF_TAB_H_

#include <yaf/autoinc.h>
#include <yaf/yafcore.h>
#include <yaf/decode.h>


struct yfFlowTab_st;
/**
 * A flow table. Opaque. Create with yfFlowTabAlloc() and free with
 * yfFlowTabFree().
 */
typedef struct yfFlowTab_st yfFlowTab_t;

/**
 *yfFlowTabAlloc
 *
 * Allocate a flow table.
 *
 * @param idle_ms   idle timeout in milliseconds. A flow that receives no
 *                  packets for the idle timeout is assumed to be complete.
 * @param active_ms active timeout in milliseconds. The maximum duration of a
 *                  flow is the active timeout; additional packets
 *                  for the same flow will be counted as part of a new flow.
 * @param max_flows maximum number of active flows. Flows exceeding this limit
 *                  will be expired in least-recent order, as if they were idle.
 *                  Used to limit resource usage of a flow table. A value of 0
 *                  disables flow count limits.
 * @param uniflow   If TRUE, export biflows using record adjacency (two uniflows
 *                  exported back-to-back. Use this for interoperability with
 *                  IPFIX collectors that do not implement RFC 5103.
 * @param silkmode  If TRUE, clamp totalOctetCount and maxTotalOctetCount to 32
 *                  bits and force active timeout on overflow. Set high order
 *                  bit in flowEndReason for each flow created on an overflow
 *                  or active timeout. Breaks IPFIX interoperability; use for
 *                  direct export to SiLK rwflowpack or flowcap.
 *
 * @param macmode   If TRUE, collect and export source and destination Mac
 *                  Addresses.
*
 * @return a new flow table.
 */
yfFlowTab_t *yfFlowTabAlloc(uint64_t        idle_ms,
                            uint64_t        active_ms,
                            uint32_t        max_flows,
                            gboolean        uniflow,
                            gboolean        silkmode,
                            gboolean        macmode,
                            gboolean        tcp_seq_enable,
                            gboolean        tcp_ack_enable,
                            gboolean        tcp_rtt_enable,
                            gboolean        tcp_rwin_enable,
                            gboolean        tcp_opt_enable,
                            gboolean        tcp_ts_enable,
                            gboolean        tcp_iat_enable);

/**
 * Free a previously allocated flow table. Discards any outstanding active
 * flows without closing or flushing them; use yfFlowTabFlushAll() before
 * yfFlowFree() to do this.
 *
 * @param flowtab a flow table allocated by yfFlowTabAlloc()
 */

void yfFlowTabFree(
    yfFlowTab_t     *flowtab);


/**
 * yfGetFlowTabStats
 * Get Flow Table Stats for Export
 *
 * @param flowtab
 * @param packets number of packets processed
 * @param flows number of flows created
 * @param rej_pkts number of packets rejected due to out of sequence
 * @param peak maximum number of flows in the flow table at any 1 time
 * @param flush number of flush events called on flow table
 */
void yfGetFlowTabStats(
    yfFlowTab_t *flowtab,
    uint64_t *packets,
    uint64_t *flows,
    uint64_t *rej_pkts,
    uint32_t *peak,
    uint32_t *flush);

/**
 * Add a decoded packet buffer to a given flow table. Adds the packet to
 * the flow to which it belongs, creating a new flow if necessary. Causes
 * the flow to which it belongs to time out if it is longer than the active
 * timeout.  Closes the flow if the flow closure conditions (TCP RST, TCP FIN
 * four-way teardown) are met.
 *
 * @param flowtab   flow table to add the packet to
 * @param pbuf      packet buffer containing decoded packet to add.
 */

void yfFlowPBuf(
    yfFlowTab_t                 *flowtab,
    yfPBuf_t                    *pbuf);

/**
 * Flush closed flows in the given flow table to the given IPFIX Message
 * Buffer. Causes any idle flows to time out, removing them from the active
 * flow table; also enforces the flow table's resource limit. If close is
 * TRUE, additionally closes all active flows and flushes as well.
 *
 * @param yfContext YAF thread context structure, holds pointers for the
 *                  flowtable from which to flush flows and the fbuf, the
 *                  destination to which the flows should be flushed
 * @param close     close all active flows before flushing
 * @param err       An error description pointer; must not be NULL.
 * @return TRUE on success, FALSE otherwise.
 */

gboolean yfFlowTabFlush(
    void            *yfContext,
    gboolean        close,
    GError          **err);

/**
 * Get the current packet clock from a flow table.
 *
 * @param flowtab a flow table
 * @return current packet clock
 */

uint64_t yfFlowTabCurrentTime(
    yfFlowTab_t     *flowtab);

/**
 * Print flow table statistics to the log.
 *
 * @param flowtab flow table to dump stats for
 * @param timer a GTimer containing the runtime
 *              (for packet and flow rate logging). May be NULL to suppress
 *              rate logging.
 */

uint64_t yfFlowDumpStats(
    yfFlowTab_t     *flowtab,
    GTimer          *timer);

void yfFlowKeyReverse(
    yfFlowKey_t       *fwd,
    yfFlowKey_t       *rev);

#endif
