/*
 ** yafrag.h
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

/**
 * @file
 *
 * Fragment reassembly interface for YAF. [TODO - new frontmatter]
 *
 * This facility is used by the YAF flow generator.
 */

#ifndef _YAF_FRAG_H_
#define _YAF_FRAG_H_

#include <yaf/autoinc.h>
#include <yaf/decode.h>
#include <yaf/yafcore.h>

struct yfFragTab_st;
/**
 * A fragment table. Opaque. Create with yfFragTabAlloc() and free with
 * yfFragTabFree().
 */
typedef struct yfFragTab_st yfFragTab_t;

/**
 * Allocate a fragment table.
 *
 * @param idle_ms   idle timeout in milliseconds. A fragmented packet for which
 *                  no fragments are received over an idle timeout is dropped.
 *                  Most host IPv4 implementations use 30 seconds (30000); it is
 *                  recommended to use the same here.
 * @param max_frags maximum number of unreassembled fragmented packets.
 *                  Fragmented packets exceeding this limit will be dropped in
 *                  least-recent order. Used to limit resource usage of a
 *                  fragment table. A value of 0 disables fragment count limits.
 *
 * @return a new fragment table.
 */

yfFragTab_t *yfFragTabAlloc(
    uint32_t        idle_ms,
    uint32_t        max_frags);

/**
 * Free a fragment table. Discards any outstanding fragmented packets within.
 *
 * @param fragtab a fragment table.
 */

void yfFragTabFree(
    yfFragTab_t         *fragtab);

/**
 * Defragment a fragment returned by yfDecodeToPBuf(). This adds the fragment to
 * the given fragment table. If the fragment completes a fragmented packet,
 * copies the assembled packet into the given pbuf, overwriting it, and
 * returns TRUE. If the packet is not fragmented (that is, if fraginfo->frag
 * is 0), has no effect and returns TRUE.
 *
 * @param fragtab   fragment table to add fragment to
 * @param fraginfo  fragment information structure filled in by yfDecodeToPBuf()
 * @param pbuf      packet buffer. On call, contains decoded fragmented packet
 *                  to add to the fragment table. If this call returns TRUE,
 *                  on return, contains assembled packet.
 * @param pkt       pkt buffer from libpcap.  We need this to reassemble
 *                  (memcpy) TCP header fragments when payload is not enabled.
 * @param hdr_len   size of the packet buffer pkt
 * @return  TRUE if pbuf is valid and contains an assembled packet,
 *          FALSE otherwise.
 */

gboolean yfDefragPBuf(
    yfFragTab_t         *fragtab,
    yfIPFragInfo_t      *fraginfo,
    yfPBuf_t            *pbuf,
    const uint8_t       *pkt,
    size_t              hdr_len);

/**
 * Print fragment reassembler statistics to the log.
 *
 * @param fragtab fragment table to dump stats for
 * @param packetTotal total number of packets observed
 */

void yfFragDumpStats(
    yfFragTab_t         *fragtab,
    uint64_t            packetTotal);

/**
 * Get Frag Stats to yfWriteStatsRec for Stats Export
 *
 * @param fragtab pointer to fragmentation table
 * @param dropped number of expired fragments
 * @param assembled number of assembled packets
 */
void yfGetFragTabStats(
    yfFragTab_t          *fragtab,
    uint32_t             *dropped,
    uint32_t             *assembled);

#endif
