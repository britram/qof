/*
 *
 ** @file CERT_IE.h
 ** Definition of the CERT "standard" information elements extension to
 ** the IETF standard RFC 5102 information elements
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2009-2012 Carnegie Mellon University. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell, Chris Inacio, Emily Ecoff <ecoff@cert.org>
 ** <netsa-help@cert.org>
 ** ------------------------------------------------------------------------
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
 ** ------------------------------------------------------------------------
 */


#ifndef CERT_IE_H_
#define CERT_IE_H_

/**
 * IPFIX information elements in 6871/CERT_PEN space for YAF
 * these elements are included within the capabilities of YAF
 * primarily, but may be used within other CERT software as
 * well
 */
static fbInfoElement_t yaf_cert_info_elements[] = {
    FB_IE_INIT("initialTCPFlags", CERT_PEN, 14, 2, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
    FB_IE_INIT("unionTCPFlags", CERT_PEN, 15, 2, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
    FB_IE_INIT("payload", CERT_PEN, 18, FB_IE_VARLEN, FB_IE_F_REVERSIBLE),
    FB_IE_INIT("reverseFlowDeltaMilliseconds", CERT_PEN, 21, 4, FB_IE_F_ENDIAN),
    FB_IE_INIT("silkAppLabel", CERT_PEN, 33, 2, FB_IE_F_ENDIAN),
    FB_IE_INIT("payloadEntropy", CERT_PEN, 35, 1, FB_IE_F_REVERSIBLE),
    FB_IE_INIT("osName", CERT_PEN, 36, FB_IE_VARLEN, FB_IE_F_REVERSIBLE),
    FB_IE_INIT("osVersion", CERT_PEN, 37, FB_IE_VARLEN, FB_IE_F_REVERSIBLE),
    FB_IE_INIT("firstPacketBanner", CERT_PEN, 38, FB_IE_VARLEN, FB_IE_F_REVERSIBLE),
    FB_IE_INIT("secondPacketBanner", CERT_PEN, 39, FB_IE_VARLEN, FB_IE_F_REVERSIBLE),
    FB_IE_INIT("flowAttributes", CERT_PEN, 40, 2, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
    FB_IE_INIT("osFingerPrint", CERT_PEN, 107, FB_IE_VARLEN, FB_IE_F_REVERSIBLE),
    FB_IE_INIT("expiredFragmentCount", CERT_PEN, 100, 4, FB_IE_F_ENDIAN),
    FB_IE_INIT("assembledFragmentCount", CERT_PEN, 101, 4, FB_IE_F_ENDIAN),
    FB_IE_INIT("meanFlowRate", CERT_PEN, 102, 4, FB_IE_F_ENDIAN),
    FB_IE_INIT("meanPacketRate", CERT_PEN, 103, 4, FB_IE_F_ENDIAN),
    FB_IE_INIT("flowTableFlushEventCount", CERT_PEN, 104, 4, FB_IE_F_ENDIAN),
    FB_IE_INIT("flowTablePeakCount", CERT_PEN, 105, 4, FB_IE_F_ENDIAN),
    /* flow stats */
    FB_IE_INIT("smallPacketCount", CERT_PEN, 500, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
    FB_IE_INIT("nonEmptyPacketCount", CERT_PEN, 501, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
    FB_IE_INIT("dataByteCount", CERT_PEN, 502, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
    FB_IE_INIT("averageInterarrivalTime", CERT_PEN, 503, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
    FB_IE_INIT("standardDeviationInterarrivalTime", CERT_PEN, 504, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
    FB_IE_INIT("firstNonEmptyPacketSize", CERT_PEN, 505, 2, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
    FB_IE_INIT("maxPacketSize", CERT_PEN, 506, 2, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
    FB_IE_INIT("firstEightNonEmptyPacketDirections", CERT_PEN, 507, 1, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
    FB_IE_INIT("standardDeviationPayloadLength", CERT_PEN, 508, 2, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
    FB_IE_INIT("tcpUrgentCount", CERT_PEN, 509, 4, FB_IE_F_ENDIAN|FB_IE_F_REVERSIBLE),
    FB_IE_INIT("largePacketCount", CERT_PEN, 510, 4, FB_IE_F_ENDIAN|FB_IE_F_REVERSIBLE),
    FB_IE_NULL
};


#endif
