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
#include "qofrtt.h"

static size_t qof_rtt_ring_sz = 0;

typedef struct qfSeqTime_st {
    uint64_t    ms;
    uint32_t    seq;
} qfSeqTime_t;

int qfWrapGT(uint32_t a, uint32_t b) {
    return ((a > b) || ((b - a) > k2e31)) ? 1 : 0;
}

void qfRttRingSize(size_t ring_sz) {
    qof_rtt_ring_sz = ring_sz;
}

void qfRttSeqInit(yfFlowVal_t *val, uint64_t ms, uint32_t seq) {
    qfSeqTime_t *stent;

    /* set initial and final sequence number */
    val->isn = val->fsn = seq;

    /* initialize seqtime buffer if configured to do so */
    if (qof_rtt_ring_sz) {
        val->rtt.seqring = rgaAlloc(sizeof(qfSeqTime_t), qof_rtt_ring_sz);
        stent = (qfSeqTime_t *)rgaForceHead(val->rtt.seqring);
        stent->ms = ms;
        stent->seq = seq;
    }
}

/* Return true if we should sample a sequence number */
static gboolean qfRttSeqSampleP(yfFlowVal_t *sval) {
    /* keep skipping until we've eaten up the period */
     if (sval->rtt.srskipped < sval->rtt.srperiod) {
        sval->rtt.srskipped++;
        return FALSE;
    }
    
    /* calculate next period */
    //fprintf(stderr, "lf %u maxlen %u ringct %u ", sval->rtt.lastflight, sval->maxiplen, rgaCount(sval->rtt.seqring));
    sval->rtt.srperiod = (sval->rtt.lastflight / sval->maxiplen ) /
                         (qof_rtt_ring_sz + 1 - rgaCount(sval->rtt.seqring));
    if (sval->rtt.srperiod) sval->rtt.srperiod -= 1;
    //fprintf(stderr, "srp %u\n", sval->rtt.srperiod);
    
    /* and return */
    sval->rtt.srskipped = 0;
    return TRUE;
}

void qfRttSeqAdvance(yfFlowVal_t *sval, yfFlowVal_t *aval, uint64_t ms, uint32_t seq) {
    qfSeqTime_t *stent;
    
    /* set final sequence number, counting wraps */
    if (seq < sval->fsn) {
        sval->wrapct += 1;
    }
    sval->fsn = seq;

    /* Everything after this point requires RTT to be configured */
    if (!sval->rtt.seqring) return;
    
    /* Take an RTT sample if we should */
    if (qfRttSeqSampleP(sval)) {
        /* add entry if we got a new sequence number */
        stent = (qfSeqTime_t *)rgaPeekTail(sval->rtt.seqring);
        if (!stent || qfWrapGT(seq, stent->seq)) {
            stent = (qfSeqTime_t *)rgaForceHead(sval->rtt.seqring);
            stent->ms = ms;
            stent->seq = seq;
        }
    }
    
    /* Minimize RTT correction factor */
    if (aval->rtt.lacktime &&
        qfWrapGT(seq, aval->rtt.lastack))
    {
        // new seq, greater than last ack
        if (!sval->rtt.rttcorr ||
            (sval->rtt.rttcorr > (uint32_t)(ms - aval->rtt.lacktime)))
        {
            // new rtt correction term, or less than previous best
            sval->rtt.rttcorr = (uint32_t)(ms - aval->rtt.lacktime);
        }
    }
}

static void qfSmoothRtt(yfFlowVal_t *sval) {
    static const unsigned int alpha = 8;
    
    /* don't smooth if we have no correction term */
    if (!sval->rtt.rttcorr) return;
    
    uint32_t rtt = sval->rtt.rawrtt + sval->rtt.rttcorr;
    
    sval->rtt.smoothrtt = sval->rtt.smoothrtt ?
                          ((sval->rtt.smoothrtt * (alpha-1)) +
                            rtt) / alpha
                          : rtt;
}

void qfRttAck(yfFlowVal_t *aval, yfFlowVal_t *sval, uint64_t ms, uint32_t ack) {
    qfSeqTime_t *stent;

    /* track RTT if configured to do so */
    if (sval->rtt.seqring && ack > aval->rtt.lastack) {
        do {
            stent = (qfSeqTime_t *)rgaNextTail(sval->rtt.seqring);
        } while (stent && stent->seq < ack);
        
        if (stent) {
            /* calculate last RTT */
            sval->rtt.rawrtt = (uint32_t)(ms - stent->ms);
            
            /* smooth RTT */
            qfSmoothRtt(sval);
            
            /* sum and count for average */
            sval->rttsum += sval->rtt.smoothrtt;
            sval->rttcount += 1;
            
            /* minimum */
            if (!sval->rtt.minrtt || sval->rtt.minrtt > sval->rtt.smoothrtt) {
                sval->rtt.minrtt = sval->rtt.smoothrtt;
            }
        }
    }

    /* track last ACK */
    if (qfWrapGT(ack, aval->rtt.lastack)) {
        aval->rtt.lastack = ack;
        aval->rtt.lacktime = ms;
    }
    
    /* track inflight high-water mark */
    if (qfWrapGT(sval->fsn, aval->rtt.lastack)) {
        sval->rtt.lastflight = sval->fsn - aval->rtt.lastack;
        if (sval->rtt.maxflight > sval->rtt.lastflight) {
            sval->rtt.maxflight = sval->rtt.lastflight;
        }
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
