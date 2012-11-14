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
     
static fbInfoElement_t yaf_info_elements[] = {
     FB_IE_INIT("deltaTCPOctetCount", TCH_PEN, 300, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_INIT("deltaTCPSequenceCount", TCH_PEN, 301, 1, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
     FB_IE_NULL
}

#endif