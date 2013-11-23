/**
 ** qofseq.c
 ** Sequence number tracking functions for QoF
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2013      Brian Trammell. All Rights Reserved
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell <brian@trammell.ch>
 ** ------------------------------------------------------------------------
 ** QoF is made available under the terms of the
 ** GNU General Public License (GPL) Version 2, June 1991
 ** ------------------------------------------------------------------------
 */

#define _YAF_SOURCE_

#include <qof/qofseq.h>
#include <qof/decode.h>

#if QF_DEBUG_SEQ
#include <qof/streamstat.h>

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

static void qfSeqGapPush(qfSeq_t *qs, uint32_t a, uint32_t b, uint16_t mss) {
    qs->ooo += ((b - a) / (mss + 1)) + 1;
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

static int qfSeqGapFill(qfSeq_t *qs, uint32_t a, uint32_t b) {
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
        return 1;
    } else {
        return 0;
    }
    
}

static void qfCountLoss(qfSeq_t *qs, qfRtt_t *rtt, uint32_t ms) {

    /* only count one loss indication per RTT */
    if (ms > (qs->losslms + rtt->val.val)) {
        qs->losslms = ms;
        qs->lossct++;
    }
}

void qfSeqFirstSegment(qfSeq_t *qs, uint8_t flags, uint32_t seq, uint32_t oct,
                       uint32_t ms, uint32_t tsval, gboolean do_ts) {
    qs->isn = seq;
    qs->nsn = seq + oct + ((flags & YF_TF_SYN) ? 1 : 0) ;
    qs->advlms = ms;
    if (do_ts && tsval) {
        qs->initlms = ms;
        qs->initsval = tsval;
    }
}

int qfSeqSegment(qfSeq_t *qs, qfRtt_t *rtt, uint16_t mss,
                 uint8_t flags, uint32_t seq, uint32_t oct,
                 uint32_t ms, uint32_t tsval,
                 gboolean do_ts, gboolean do_iat) {

    uint32_t lastms = 0;
    
    /* Empty segments don't count */
    if (!oct) return 0;
    
    if (qfWrapCompare(seq, qs->nsn) < 0) {
        /* Sequence less than NSN: fill */
        if (seq - qs->nsn > qs->maxooo) {
            qs->maxooo = seq - qs->nsn;
        }
        if (qfSeqGapFill(qs, seq, seq + oct)) {
            qfCountLoss(qs, rtt, ms);
        }
    } else {
        /* Sequence beyond NSN: push */
        if (seq != qs->nsn) {
            qfSeqGapPush(qs, qs->nsn, seq, mss);

            /* signal loss for burst tracking */
            qfCountLoss(qs, rtt, ms);
            
            /* track max out of order */
            if (seq - qs->nsn > qs->maxooo) {
                qs->maxooo = seq - qs->nsn;
            }
        }
        
        /* Detect wrap */
        if (seq + oct < qs->nsn) {
            qs->wrapct++;
        }
        
        /* Determine next sequence number */
        qs->nsn = seq + oct;
        
        /* track timestamp frequency */
        if (do_ts && tsval) {
            /* increment wrap counters if necessary */
            if (tsval < qs->advtsval) {
                qs->tsvalwrap++;
            }
            if (ms < qs->advlms) {
                qs->lmswrap++;
            }

            /* save current value */
            qs->advtsval = tsval;
        }

        /* and advance time */
        lastms = qs->advlms;
        qs->advlms = ms;

        /* calculate interarrival/interdeparture time of advancing segments */
        if (do_iat) {
            uint32_t iat = 0, idt = 0, hz = 0;
            
            iat = ms - lastms;
            sstMeanAdd(&qs->seg_iat, iat);
            if (do_ts && tsval && (hz = qfTimestampHz(qs))) {
                idt = (1000 * (tsval - qs->advtsval)) / hz;
                sstMeanAdd(&qs->seg_variat, iat - idt);
            }
        }
        
        return 1;
    }
    
    /* if we're here, we didn't advance */
    return 0;
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
//    fprintf(stderr,"seqCountLost %p prev_seqlost %u isn %u nsn %u len %u\n",
//            qs, qs->seqlost, qs->isn, qs->nsn, qs->nsn - qs->isn);
    for (i = 0; i < QF_SEQGAP_CT && !qfSeqGapEmpty(qs->gaps, i); i++) {
        qs->seqlost += (uint32_t)(qs->gaps[i].b - qs->gaps[i].a);
//            fprintf(stderr, "\t%u-%u (%u) (%u)\n",
//                    qs->gaps[i].a,  qs->gaps[i].b,
//                    qs->gaps[i].b - qs->gaps[i].a,
//                    qs->nsn - qs->gaps[i].b);
        qs->gaps[i].a = 0;
        qs->gaps[i].b = 0;
    }

    return qs->seqlost;

}

uint32_t qfTimestampHz(qfSeq_t *qs)
{
    uint64_t val_interval =
    (((uint64_t)qs->tsvalwrap * k2e32) + qs->advtsval - qs->initsval);
    uint64_t lms_interval =
    (((uint64_t)qs->lmswrap * k2e32) + qs->advlms - qs->initlms);
    
    if (lms_interval && qs->initsval && qs->advtsval) {
        return (uint32_t)(val_interval * 1000 / lms_interval);
    } else {
        return 0;
    }
}

void qfSeqContinue(qfSeq_t *cont_seq, qfSeq_t *orig_seq)
{
    
}