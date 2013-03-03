/**
 ** qofctx.h
 ** QoF configuration
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2007-2012 Carnegie Mellon University. All Rights Reserved.
 ** Copyright (C) 2013      Brian Trammell. All Rights Reserved
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

#ifndef _QOF_CTX_H_
#define _QOF_CTX_H_

#include <yaf/autoinc.h>
#include <yaf/yaftab.h>
#include <yaf/yafrag.h>
#include <yaf/decode.h>
#include <yaf/ring.h>
#include "qofifmap.h"
#include <airframe/airlock.h>

#include <yaml.h>

/* YAF Configuration structure */
typedef struct yfConfig_st {
    char            *inspec;            // Input specifier
    char            *livetype;          // Packet capture driver
    char            *outspec;           // Output specifier
    char            *bpf_expr;          // libpcap BPF filter expression
    gboolean        lockmode;           // true = use lock files
    gboolean        ipfixNetTrans;      // ??
    gboolean        noerror;            // ??
    gboolean        dagInterface;       // ??
    gboolean        pcapxInterface;     // ??
    gboolean        macmode;            // Emit Layer 2 information
    gboolean        silkmode;           // Enable SiLK compatibility
    gboolean        nostats;            // Disable statistics
    gboolean        statsmode;          // ??
    gboolean        deltamode;          // Export counters as deltas
    uint32_t        ingressInt;         // Constant ingress interface
    uint32_t        egressInt;          // Constant egress interface
    uint64_t        stats;              // ??
    uint64_t        rotate_ms;          // ??
    /* in seconds - convert to ms in yaf.c */
    uint64_t        yaf_udp_template_timeout;   // UDP template retransmission
    uint32_t        odid;                       // Observation domain ID
    fbConnSpec_t    connspec;                   // fixbuf connection specifier
    } yfConfig_t;

#define YF_CONFIG_INIT {NULL, NULL, NULL, NULL, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, 0, 0, 300, 0, 600, 0, FB_CONNSPEC_INIT}

typedef struct yfContext_st {
    /** Configuration */
    yfConfig_t          *cfg;
    /** Packet source */
    void                *pktsrc;
    /** Packet ring buffer */
    rgaRing_t           *pbufring;
    /** Decoder */
    yfDecodeCtx_t       *dectx;
    /** Flow table */
    yfFlowTab_t         *flowtab;
    /** Fragment table */
    yfFragTab_t         *fragtab;
    /** Interface map */
    qfIfMap_t           ifmap;              // Interface map
    /** Output rotation state */
    uint64_t            last_rotate_ms;
    /** Output lock buffer */
    AirLock             lockbuf;
    /** Output IPFIX buffer */
    fBuf_t              *fbuf;
    /** UDP last template send time (in ms) */
    uint64_t            lastUdpTempTime;
    /** Error description */
    GError              *err;
} yfContext_t;

#define YF_CTX_INIT {NULL, NULL, NULL, NULL, NULL, NULL, QF_IFMAP_INIT, 0, AIR_LOCK_INIT, NULL, 0, NULL}

gboolean qfParseYamlConfig(yfContext_t           *ctx,
                           const char            *filename,
                           GError                **err);

#endif
