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


#if 0
static int qfSeqGapValidate(qfSeq_t *qs) {
    int i;
    char *err = NULL;

    for (i = 0; i < QF_SEQGAP_CT; i++) {
        if (qs->gaps[i].a && qs->gaps[i].b) {
            if (qfWrapCompare(qs->gaps[i].b, qs->gaps[i].a) < 1) {
                err = "invalid";
                break;
            }
            if (i && (qfWrapCompare(qs->gaps[i].a, qs->gaps[i-1].b) >= 0)) {
                err = "inverted";
                break;
            }
        }
    }
    
    if (!err) return 1;
    
    fprintf(stderr, "%8s gap at %u in %p (nsn %u):\n", err, i, qs, qs->nsn);
    
    for (i = 0; i < QF_SEQGAP_CT; i++) {
        fprintf(stderr, "\t%u-%u (%u) (%d)\n",
                qs->gaps[i].a,  qs->gaps[i].b,
                qs->gaps[i].b - qs->gaps[i].a,
                i ? (qs->gaps[i-1].b - qs->gaps[i].a) : 0);
    }

    return 0;
}
#endif


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
    }

}

static void qfSeqGapFill(qfSeq_t *qs, uint32_t a, uint32_t b) {
    int i;
    uint32_t rtxoct = 0, nexa, nexb;

    /* Segment might fill multiple gaps, so iterate until
       we've distributed the entire segment */
    while (qfWrapCompare(b, a) > 0) {
        
        /* Seek to the next applicable gap */
        i = 0;
        while ((i < QF_SEQGAP_CT) && !qfSeqGapEmpty(qs->gaps, i) &&
               (qfWrapCompare(a, qs->gaps[i].a)) < 0) i++;
        
        /* If we're off the edge of the gapstack, this is pure RTX. */
        if (i == QF_SEQGAP_CT || qfSeqGapEmpty(qs->gaps, i)) {
            rtxoct += b - a;
            break;
        }
        
        /* Everything greater than the gap is also RTX */
        if (qfWrapCompare(b, qs->gaps[i].b) > 0) {
            if (qfWrapCompare(a, qs->gaps[i].b) > 0) {
                rtxoct += b - a;
                break;
            } else {
                rtxoct += b - qs->gaps[i].b;
                b = qs->gaps[i].b;
            }
        }

        /* Check to see if we'll need to iterate */
        if (qfWrapCompare(a, qs->gaps[i].a) < 0) {
            /* A is less than the gap; prepare to iterate */
            nexa = a;
            nexb = b;
            a = qs->gaps[i].a;
        } else {
            /* A and B within or on gap edge; terminate iteration */
            nexa = 0;
            nexb = 0;
        }

        /* Check for overlap within gap and fill */
        if ((a == qs->gaps[i].a) && (b == qs->gaps[i].b)) {
            /* Completely fill gap */
            qfSeqGapShift(qs->gaps, i);
            break;
        } else if (b == qs->gaps[i].b) {
            /* A within gap, B on edge; fill on the high side */
            qs->gaps[i].b = a;
            break;
        } else if (a == qs->gaps[i].a) {
            /* B within gap, A on edge; fill on the high side */
            qs->gaps[i].a = b;
            break;
        } else {
            /* A and B within gap; split the gap */
            qs->seqlost += qfSeqGapUnshift(qs->gaps, i);
            qs->gaps[i].a = b;
            qs->gaps[i+1].b = a;
        }
        
        /* Modify segment and iterate */
        a = nexa;
        b = nexb;
    }
    
    /* Done. Count retransmit */
    if (rtxoct) {
        qs->rtx++;
    }
    
}

void qfSeqFirstSegment(qfSeq_t *qs, uint32_t seq, uint32_t oct) {
    qs->isn = seq;
    qs->nsn = seq + oct;
}

void qfSeqSegment(qfSeq_t *qs, uint32_t seq, uint32_t oct) {
    /* Empty segments don't count */
    if (!oct) return;
    
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

    return qs->seqlost;

}