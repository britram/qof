/**
 ** qofrtt.c
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

#define _YAF_SOURCE_
#include <yaf/autoinc.h>
#include <yaf/qofrtt.h>

static size_t qof_rtt_ring_sz = 0;

typedef struct qfSeqTime_st {
    uint64_t    ms;
    uint32_t    seq;
} qfSeqTime_t;

void qfRttRingSize(size_t ring_sz) {
    qof_rtt_ring_sz = ring_sz;
}

void qfRttSeqInit(yfFlowVal_t *val, uint64_t ms, uint32_t seq) {
    qfSeqTime_t *stent;

    /* set initial and final sequence number */
    val->isn = val->fsn = seq;

    /* initialize seqtime buffer if configured to do so */
    if (qof_rtt_ring_sz) {
        val->seqtime = rgaAlloc(sizeof(qfSeqTime_t), qof_rtt_ring_sz);
        stent = (qfSeqTime_t *)rgaForceHead(val->seqtime);
        stent->ms = ms;
        stent->seq = seq;
    }
}

void qfRttSeqAdvance(yfFlowVal_t *val, uint64_t ms, uint32_t seq) {
    qfSeqTime_t *stent;
    
    /* set final sequence number, counting wraps */
    if (seq < val->fsn) {
        val->wrapct += 1;
    }
    val->fsn = seq;

    /* track seqtime if configured to do so */
    if (val->seqtime) {
        stent = (qfSeqTime_t *)rgaForceHead(val->seqtime);
        stent->ms = ms;
        stent->seq = seq;
    }
}