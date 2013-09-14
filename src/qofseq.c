/**
 ** qofseq.c
 ** Sequence number tracking functions for QoF
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

#include <yaf/qofseq.h>
#include <yaf/decode.h>

#if QF_DEBUG_SEQ
#include <yaf/streamstat.h>

static sstMean_t gapstack_high;
static int debug_init = 0;

#endif

/** Constant - 2^32 (for sequence number calculations) */
static const uint64_t k2e32 = 0x100000000ULL;

static int qfWrapCompare(uint32_t a, uint32_t b) {
    return a == b ? 0 : ((a - b) & 0x80000000) ? -1 : 1;
}

static int qfSeqGapEmpty(qfSeqGap_t *sg, unsigned i) {
    return !(sg[i].a || sg[i].b);
}

static void qfSeqGapShift(qfSeqGap_t *sg, unsigned i) {
    memmove(&sg[i], &sg[i+1], sizeof(qfSeqGap_t)*(QF_SEQGAP_CT - i));
    sg[QF_SEQGAP_CT - 1].a = 0;
    sg[QF_SEQGAP_CT - 1].b = 0;
}

static uint32_t qfSeqGapUnshift(qfSeqGap_t *sg, unsigned i) {
    uint32_t lost = sg[QF_SEQGAP_CT - 1].b - sg[QF_SEQGAP_CT - 1].a;
    memmove(&sg[i+1], &sg[i], (QF_SEQGAP_CT - i - 1) * sizeof(qfSeqGap_t));
    return lost;
}

static void qfSeqGapPush(qfSeq_t *qs, uint32_t a, uint32_t b) {
    qs->ooo++;
    if (!qfSeqGapEmpty(qs->gaps, 0) && (a == qs->gaps[0].a)) {
        /* Special case: extend an existing gap */
        qs->gaps[0].b = b;
    } else {
        /* Push a new gap cell */
        qs->seqlost += qfSeqGapUnshift(qs->gaps, 0);
        qs->gaps[0].a = a;
        qs->gaps[0].b = b;
#if QF_DEBUG_SEQ
        qs->gapcount++;
        if (qs->gapcount > qs->highwater) qs->highwater = qs->gapcount;
#endif // QF_DEBUG_SEQ
    }
}

static void qfSeqGapFill(qfSeq_t *qs, uint32_t a, uint32_t b) {
    int i = 0;
    
    /* seek to gap to fill */
    while (i < QF_SEQGAP_CT &&
           !qfSeqGapEmpty(qs->gaps, i) &&
            qfWrapCompare(a, qs->gaps[i].a) < 0 &&
            qfWrapCompare(b, qs->gaps[i].a) < 0) i++;

    if (i == QF_SEQGAP_CT || qfSeqGapEmpty(qs->gaps, i)) {
        /* no gap to fill, signal rtx */
        qs->rtx++;
    } else if (a > qs->gaps[i].b) {
        /* fill after gap, signal rtx */
        qs->rtx++;
    } else if ((a == qs->gaps[i].a) && (b == qs->gaps[i].b)) {
        /* fill gap completely */
        qfSeqGapShift(qs->gaps, i);
#if QF_DEBUG_SEQ
        qs->gapcount--;
#endif // QF_DEBUG_SEQ
    } else if (a == qs->gaps[i].a) {
        /* fill gap on low side */
        qs->gaps[i].a = b;
    } else if (b == qs->gaps[i].b) {
        /* fill gap on high side */
        qs->gaps[i].b = a;
    } else {
        /* split gap in middle */
        qs->seqlost += qfSeqGapUnshift(qs->gaps, i);
        qs->gaps[i].a = b;
        qs->gaps[i+1].b = a;
#if QF_DEBUG_SEQ
        qs->gapcount++;
        if (qs->gapcount > qs->highwater) qs->highwater = qs->gapcount;
#endif // QF_DEBUG_SEQ
    }
}

void qfSeqFirstSegment(qfSeq_t *qs, uint32_t seq, uint32_t oct) {
#if QF_DEBUG_SEQ
    if (!debug_init) {
        fprintf(stderr, "initializing gapstack debug structures\n");
        sstMeanInit(&gapstack_high);
        debug_init++;
    }
#endif // QF_DEBUG_SEQ
    qs->isn = seq;
    qs->nsn = seq + oct;
}

void qfSeqSegment(qfSeq_t *qs, uint32_t seq, uint32_t oct) {
    if (qfWrapCompare(seq, qs->nsn) < 0) {
        if (seq - qs->nsn > qs->maxooo) {
            qs->maxooo = seq - qs->nsn;
        }
        qfSeqGapFill(qs, seq, seq + oct);
    } else {
        if (seq != qs->nsn) qfSeqGapPush(qs, seq, seq + oct);
        if (seq + oct < qs->nsn) qs->wrapct++;
        qs->nsn = seq + oct;
    }
}

uint64_t qfSeqCount(qfSeq_t *qs, uint8_t flags) {
    uint64_t sc = qs->nsn - qs->isn + (k2e32 * qs->wrapct);
    if (sc) sc -= 1; // remove one: nsn is the next expected sequence number
    if (sc & (flags & YF_TF_SYN) && sc) sc -= 1; // remove one for the syn
    if (sc & (flags & YF_TF_FIN) && sc) sc -= 1; // remove one for the fin
    return sc;
}

uint32_t qfSeqCountLost(qfSeq_t *qs) {
    int i;
    
    /* iterate over gaps adding to loss */
    for (i = 0; i < QF_SEQGAP_CT && !qfSeqGapEmpty(qs->gaps, i); i++) {
        qs->seqlost += qs->gaps[i].b - qs->gaps[i].a;
        qs->gaps[i].a = 0;
        qs->gaps[i].b = 0;
    }

#if QF_DEBUG_SEQ
    fprintf(stderr, "gapstack count %u hw %u\n", qs->gapcount, qs->highwater);
    if (!qs->seqlost) {
        sstMeanAdd(&gapstack_high, qs->highwater);
        fprintf(stderr, "    mean hw %4.2f, max hw %3u\n", gapstack_high.mean, gapstack_high.max);
    }

#endif // QF_DEBUG_SEQ

    return qs->seqlost;

}