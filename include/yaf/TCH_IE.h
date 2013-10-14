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
     FB_IE_INIT("maxTcpSequenceJump", TCH_PEN, 1026, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("minTcpRttMilliseconds", TCH_PEN, 1029, 4, FB_IE_F_ENDIAN),
     FB_IE_INIT("lastTcpRttMilliseconds", TCH_PEN, 1030, 4, FB_IE_F_ENDIAN),
     FB_IE_INIT("ectMarkCount", TCH_PEN, 1031, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("ceMarkCount", TCH_PEN, 1032, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("declaredTcpMss", TCH_PEN, 1033, 2, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("observedTcpMss", TCH_PEN, 1034, 2, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("tcpSequenceLossCount", TCH_PEN, 1035, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("tcpOutOfOrderCount", TCH_PEN, 1036, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("tcpLossBurstCount", TCH_PEN, 1038, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("qofTcpCharacteristics", TCH_PEN, 1039, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("tcpDupAckCount", TCH_PEN, 1040, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("tcpSelAckCount", TCH_PEN, 1041, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("minTcpRwin", TCH_PEN, 1042, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("meanTcpRwin", TCH_PEN, 1043, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("maxTcpRwin", TCH_PEN, 1044, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("tcpReceiverStallCount", TCH_PEN, 1045, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("tcpRttSampleCount", TCH_PEN, 1046, 4, FB_IE_F_ENDIAN),
     FB_IE_INIT("tcpTimestampFrequency", TCH_PEN, 1047, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("minTcpChirpMilliseconds", TCH_PEN, 1048, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("maxTcpChirpMilliseconds", TCH_PEN, 1049, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("minTcpIOTMilliseconds", TCH_PEN, 1050, 2, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("maxTcpIOTMilliseconds", TCH_PEN, 1051, 2, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
    
};

#endif