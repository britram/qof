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
        val->rtt.seqtime = rgaAlloc(sizeof(qfSeqTime_t), qof_rtt_ring_sz);
        stent = (qfSeqTime_t *)rgaForceHead(val->rtt.seqtime);
        stent->ms = ms;
        stent->seq = seq;
    }
}

void qfRttSeqAdvance(yfFlowVal_t *sval, yfFlowVal_t *aval, uint64_t ms, uint32_t seq) {
    qfSeqTime_t *stent;
    
    /* set final sequence number, counting wraps */
    if (seq < sval->fsn) {
        sval->wrapct += 1;
    }
    sval->fsn = seq;

    /* do RTT stuff if enabled */
    if (sval->rtt.seqtime) {
        /* add entry if we got a new sequence number */
        stent = (qfSeqTime_t *)rgaPeekTail(sval->rtt.seqtime);
        if ((seq > stent->seq) || ((stent->seq - seq) > k2e31)) {
            stent = (qfSeqTime_t *)rgaForceHead(sval->rtt.seqtime);
            stent->ms = ms;
            stent->seq = seq;
        }
        
        /* generate a new RTT correction if necessary */
        if (!aval->rtt.rttcorr &&
            aval->rtt.lastack &&
            seq > aval->rtt.lastack)
        {
            aval->rtt.rttcorr = (uint32_t)(ms - aval->rtt.lacktime);
        }
    }
}

static void qfSmoothRtt(yfFlowVal_t *sval) {
    static const unsigned int alpha = 8;
    
    sval->rtt.smoothrtt = sval->rtt.smoothrtt ?
                          ((sval->rtt.smoothrtt * (alpha-1)) +
                            sval->rtt.lastrtt) / alpha
                          : sval->rtt.lastrtt;
}

void qfRttAck(yfFlowVal_t *aval, yfFlowVal_t *sval, uint64_t ms, uint32_t ack) {
    qfSeqTime_t *stent;

    /* track RTT if configured to do so */
    if (sval->rtt.seqtime && ack > aval->rtt.lastack) {
        do {
            stent = (qfSeqTime_t *)rgaNextTail(sval->rtt.seqtime);
        } while (stent && stent->seq < ack);
        
        if (stent) {
            /* calculate last RTT */
            sval->rtt.lastrtt = (uint32_t)(ms - stent->ms);
            
            /* smooth RTT */
            qfSmoothRtt(sval);
            
            /* sum and count for average */
            /* FIXME should we just export rtt.smoothrtt instead? */
            sval->rttsum += sval->rtt.lastrtt;
            sval->rttcount += 1;
            
            /* maximum */
            if (sval->rtt.lastrtt > sval->rtt.maxrtt) {
                sval->rtt.maxrtt = sval->rtt.lastrtt;
            }
        }
    }

    /* track last ACK */
    aval->rtt.lastack = ack;
    aval->rtt.lacktime = ms;
    
    /* track inflight high-water mark */
    // FIXME does not handle wraparound
    // FIXME also doesn't handle a valid ACK value of 0
    if ((sval->fsn > aval->rtt.lastack) &&
        ((sval->fsn - aval->rtt.lastack) > sval->rtt.maxflight))
    {
        sval->rtt.maxflight = sval->fsn - aval->rtt.lastack;
    }
}

unsigned int qfPathDistance(yfFlowVal_t *val) {
    // minttl mod 64
    return val->minttl & 0x3F;
}

unsigned int qfCurrentRtt(yfFlow_t *f) {
    if (f->val.rtt.smoothrtt > f->rval.rtt.smoothrtt) {
        return f->val.rtt.smoothrtt;
    } else {
        return f->rval.rtt.smoothrtt;
    }
}

void qfLose(yfFlow_t *f, yfFlowVal_t *val, uint64_t ms) {
    /* begin new burst? */
    if (val->rtt.blosstime < ms + qfCurrentRtt(f)) {
        val->rtt.blosstime = ms;
        val->rtt.blosscount += 1;
        if (val->rtt.maxbloss < val->rtt.lastbloss) val->rtt.maxbloss = val->rtt.lastbloss;
        val->rtt.lastbloss = 0;
    }
    
    /* add to current burst */
    val->rtt.lastbloss++;
}
