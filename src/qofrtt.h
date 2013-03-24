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

/**
 * Set the size of the per-flow sequence sample buffer for RTT calculation.
 * Must be called to enable RTT calculation and associated 
 * 
 * @param ring_sz number of samples in ring buffer
 */
void qfRttRingSize(size_t ring_sz);

/**
 * Determine if A is greater than B, accounting for wraparound.
 *
 * @param a value to compare
 * @param b value to compare
 */
int qfWrapGT(uint32_t a, uint32_t b);

/**
 * Initialize sequence number tracking for a flow. 
 *
 * @param sval value structure for the flow's forward direction.
 * @param ms   time in milliseconds of first packet of flow.
 * @param seq  sequence number of first packet of flow.
 */
void qfRttSeqInit(yfFlowVal_t *sval, uint64_t ms, uint32_t seq);


/**
 * Advance sequence number on a flow. May lead to a sequence number sample 
 * being taken for RTT calculation. Calculates RTT correction term,
 * updates maximum inflight octets.
 *
 * @param sval value structure for the flow's forward (SEQ) direction.
 * @param aval value structure for the flow's reverse (ACK) direction.
 * @param ms   time in milliseconds of observed packet.
 * @param seq  sequence number of observed packet; must be greater than FSN.
 */
void qfRttSeqAdvance(yfFlowVal_t *sval, yfFlowVal_t *aval, uint64_t ms, uint32_t seq);

/**
 * Advance last ack on a flow. Updates RTT, calculates current inflight octets.
 *
 * @param aval value structure for the flow's reverse (ACK) direction.
 * @param sval value structure for the flow's forward (SEQ) direction.
 * @param ms   time in milliseconds of observed packet.
 * @param ack  ack number on observed packet (next expected octet at receiver)
 */
void qfRttAck(yfFlowVal_t *aval, yfFlowVal_t *sval, uint64_t ms, uint32_t ack);

unsigned int qfPathDistance(yfFlowVal_t *val);
unsigned int qfCurrentRtt(yfFlow_t *f);

void qfLose(yfFlow_t *f, yfFlowVal_t *val, uint64_t ms);

#endif