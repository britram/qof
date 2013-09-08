/**
 ** qofrtt.c
 ** RTT estimation data structures and function prototypes for QoF
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2013      Brian Trammell. All Rights Reserved
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell <brian@trammell.ch>
 ** ------------------------------------------------------------------------
 ** QoF is made available under the terms of the GNU Public License (GPL)
 ** Version 2, June 1991
 ** ------------------------------------------------------------------------
 */

#define _YAF_SOURCE_
#include <yaf/qofrtt.h>
#include <yaf/yaftab.h>


void qfRttSegment(qfRtt_t           *rtt,
                  uint32_t          seq,
                  uint32_t          ack,
                  uint32_t          tsval,
                  uint32_t          tsecr,
                  uint8_t           tcpflags,
                  unsigned          reverse)
{
    
}