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

void qfRttAck(yfFlowVal_t *aval, yfFlowVal_t *sval, uint64_t ms, uint32_t ack) {
    qfSeqTime_t *stent;

    /* track RTT if configured to do so */
    if (sval->seqtime && ack > aval->lack) {
        do {
            stent = (qfSeqTime_t *)rgaNextTail(sval->seqtime);
        } while (stent && stent->seq < ack);
        
        if (stent) {
            /* last RTT */
            sval->lrtt = (uint32_t)(ms - stent->ms);
            
            /* smoothed RTT */
            /* FIXME hardcoded for alpha = 1/8 -- what is this, 1990? */
            sval->srtt = sval->srtt ? ((sval->srtt * 7) + sval->lrtt)/8
                                    : sval->lrtt;
            
            /* sum and count for average */
            /* FIXME should we just export srtt instead? */
            sval->rttsum += sval->lrtt;
            sval->rttcount += 1;
            
            /* maximum */
            if (sval->lrtt > sval->maxrtt) sval->maxrtt = sval->lrtt;
        }
    }

    /* track last ACK */
    aval->lack = ack;
    
    /* track inflight high-water mark */
    // FIXME does not handle wraparound
    // FIXME also doesn't handle a valid ACK value of 0
    if ((sval->fsn > aval->lack) &&
        ((sval->fsn - aval->lack) > sval->maxflight))
    {
        sval->maxflight = sval->fsn - aval->lack;
    }
}

unsigned int qfPathDistance(yfFlowVal_t *val) {
    // minttl mod 64
    return val->minttl & 0x3F;
}

unsigned int qfCurrentRtt(yfFlow_t *f) {
    /* FIXME incorporate path distance? */
    if (f->val.srtt > f->rval.srtt) {
        return f->val.srtt;
    } else {
        return f->rval.srtt;
    }
}

void qfLose(yfFlow_t *f, yfFlowVal_t *val, uint64_t ms) {
    /* begin new burst? */
    if (val->blossbegin < ms + qfCurrentRtt(f)) {
        val->blossbegin = ms;
        val->blosscount += 1;
        if (val->maxbloss < val->lbloss) val->maxbloss = val->lbloss;
        val->lbloss = 0;
    }
    
    /* add to current burst */
    val->lbloss++;
}
