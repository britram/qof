/**
 ** @file qofrtt.h
 **
 ** RTT calculation and sequence number timing for QoF
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

#ifndef _QOF_RTT_H_
#define _QOF_RTT_H_

#include <yaf/autoinc.h>
#include <yaf/yaftab.h>

void qfRttRingSize(size_t ring_sz);

void qfRttSeqInit(yfFlowVal_t *val, uint64_t ms, uint32_t seq);
void qfRttSeqAdvance(yfFlowVal_t *val, uint64_t ms, uint32_t seq);
void qfRttAck(yfFlowVal_t *aval, yfFlowVal_t *sval, uint64_t ms, uint32_t ack);

#endif