/**
 ** @file TCH_IE.h
 ** Definition of the trammell.ch PEN 35566 information elements
 ** for use in QoF
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2012 Brian Trammell. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Author: Brian Trammell <brian@trammell.ch>
 ** ------------------------------------------------------------------------
 ** QoF is made available under the terms of the GNU Public License (GPL) 
 ** Version 2, June 1991
 ** ------------------------------------------------------------------------
 */

#ifndef TCH_IE_H_
#define TCH_IE_H_

#define TCH_PEN 35566
     
static fbInfoElement_t yaf_tch_info_elements[] = {
     FB_IE_INIT("tcpSequenceCount", TCH_PEN, 1024, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("tcpRetransmitCount", TCH_PEN, 1025, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("maxTcpReorderSize", TCH_PEN, 1026, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("maxTcpFlightSize", TCH_PEN, 1027, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("meanTcpRttMilliseconds", TCH_PEN, 1028, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("minTcpRttMilliseconds", TCH_PEN, 1029, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("lastTcpRttMilliseconds", TCH_PEN, 1030, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("ectMarkCount", TCH_PEN, 1031, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("ceMarkCount", TCH_PEN, 1032, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("declaredTcpMss", TCH_PEN, 1033, 2, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("observedTcpMss", TCH_PEN, 1034, 2, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("tcpSequenceLossCount", TCH_PEN, 1035, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("tcpReorderCount", TCH_PEN, 1036, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
};

#endif