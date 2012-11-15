/**
 ** @file TCH_IE.h
 ** Definition of the trammell.ch PEN 35566 information elements
 ** for use in QoF
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2012 Brian Trammell. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Author: Brian Trammell
 ** ------------------------------------------------------------------------
 ** QoF is made available under the terms of the GNU Public License (GPL) 
 ** Version 2, June 1991
 ** ------------------------------------------------------------------------
 */

#ifndef TCH_IE_H_
#define TCH_IE_H_

#define TCH_PEN 35566
     
static fbInfoElement_t yaf_tch_info_elements[] = {
     FB_IE_INIT("deltaTcpOctetCount", TCH_PEN, 1024, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("deltaTcpSequenceCount", TCH_PEN, 1025, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("meanTcpFlightSize", TCH_PEN, 1026, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("maxTcpFlightSize", TCH_PEN, 1027, 4, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("meanTcpRTTMilliseconds", TCH_PEN, 1028, 2, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("maxTcpRTTMilliseconds", TCH_PEN, 1029, 2, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_NULL
}

#endif