/**
 ** qofdyn.c
 ** TCP dynamics tracking data structures and algorithms for QoF
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
#include <yaf/qofdyn.h>
#include <yaf/yaftab.h>

#include "qofdyntmi.h"

/** Constant - 2^32 (for sequence number calculations) */
static const uint64_t k2e32 = 0x100000000ULL;

static const float        kFlightBreakIATDev = 2.0;
static const uint32_t     kFlightMinSegments = 10;

static uint32_t qf_dyn_bincap = 0;
static uint32_t qf_dyn_binscale = 1;

static uint64_t qf_dyn_stat_ringover = 0;

int qfSeqCompare(uint32_t a, uint32_t b) {
    return a == b ? 0 : ((a - b) & 0x80000000) ? -1 : 1;
}

/* *******************************************************************
 * REMOVING SeqTime/SeqRing in rttwalk branch - begin cut
 * *******************************************************************/
#if 0
/** Sequence number / time tuple */
struct qfSeqTime_st {
    uint32_t    ms;
    uint32_t    seq;
};

void qfSeqRingInit(qfSeqRing_t              *sr,
                   uint32_t                 capacity)
{
    /* zero structure */
    memset(sr, 0, sizeof(*sr));
    
    /* allocate bin array */
    sr->bincount = capacity;
    sr->bin = yg_slice_alloc0(sr->bincount * sizeof(qfSeqTime_t));
}

void qfSeqRingFree(qfSeqRing_t              *sr)
{
    if (sr->bin) yg_slice_free1(sr->bincount * sizeof(qfSeqTime_t), sr->bin);
}

void qfSeqRingAddSample(qfSeqRing_t         *sr,
                        uint32_t            seq,
                        uint32_t            ms)
{
    /* overrun if we need to */
    if (((sr->head + 1) % sr->bincount) == sr->tail) {
        sr->tail++;
        if (sr->tail >= sr->bincount) sr->tail = 0;
        qf_dyn_stat_ringover++;
    }
    
    /* insert sample */
    sr->bin[sr->head].seq = seq;
    sr->bin[sr->head].ms = ms;
    sr->head++;
    if (sr->head >= sr->bincount) sr->head = 0;
}

uint32_t qfSeqRingRTT(qfSeqRing_t           *sr,
                      uint32_t              ack,
                      uint32_t              ms)
{
    uint32_t rtt;
        
    while (sr->head != sr->tail &&
           qfSeqCompare(sr->bin[sr->tail].seq, ack-1) < 0)
    {
        sr->tail++;
        if (sr->tail >= sr->bincount) sr->tail = 0;
    }

    if (sr->head == sr->tail) return 0;
    
    rtt = ms - sr->bin[sr->tail].ms;
    sr->tail++;
    if (sr->tail >= sr->bincount) sr->tail = 0;
    return rtt;
}

static size_t qfSeqRingAvail(qfSeqRing_t *sr)
{
    ssize_t occ = sr->head - sr->tail;
    if (occ < 0) occ += sr->bincount;
    return sr->bincount - occ;
}

static int qfDynHasSeqring(qfDyn_t *qd) { return qd->sr.bin != 0; }

static int qfDynSeqSampleP(qfDyn_t     *qd,
                           uint64_t     ms)
{
    /* don't sample without seqring */
    if (!qfDynHasSeqring(qd)) {
        return 0;
    }
    
    /* don't sample higher than sample rate */
    if ((qd->sr.bin[qd->sr.head].ms + kSeqSamplePeriodMs) > ms)
    {
        return 0;
    }
    
    /* don't sample until we've waited out the sample period */
    if (qd->sr_skip < qd->sr_period) {
        qd->sr_skip++;
        return 0;
    }
    
    /* good to sample, calculate next period */
    if ((qd->dynflags & QF_DYN_SEQINIT) && (qd->dynflags & QF_DYN_ACKINIT)) {
        qd->sr_period = ((qd->nsn - qd->fan) / qd->mss) / qfSeqRingAvail(&qd->sr);
        if (qd->sr_period) qd->sr_period--;        
    } else {
        qd->sr_period = 0;
    }
    qd->sr_skip = 0;
    
    return 1;
}
#endif
/* *******************************************************************
 * REMOVING SeqTime/SeqRing in rttwalk branch - end cut
 * *******************************************************************/

void qfSeqBitsInit(qfSeqBits_t *sb, uint32_t capacity, uint32_t scale) {
    bimInit(&sb->map, capacity / scale);
    sb->scale = scale;
}

void qfSeqBitsFree(qfSeqBits_t *sb) {
    bimFree(&(sb->map));
}

static int qfDynHasSeqbits(qfDyn_t *qd) { return qd->sb.map.v != 0; }

qfSeqStat_t qfSeqBitsSegment(qfSeqBits_t *sb, uint32_t aseq, uint32_t bseq) {
    bimIntersect_t brv;
    uint32_t seqcap, maxseq;
    uint64_t bits;
    
    /* short-circuit on no seqbits */
    if (!sb) return QF_SEQ_INORDER;
    
    /* set seqbase on first segment */
    if (!sb->seqbase) {
        sb->seqbase = (aseq / sb->scale) * sb->scale;
    }
    
    /* move range forward if necessary to cover last sn, noting loss */
    seqcap = sb->map.sz * k64Bits * sb->scale;
    maxseq = sb->seqbase + seqcap;
    while (qfSeqCompare(bseq, maxseq) > 0) {
        bits = bimShiftDown(&sb->map);
        sb->lostseq_ct += ((k64Bits - bimCountSet(bits)) * sb->scale);
        sb->seqbase += sb->scale * k64Bits;
        maxseq = sb->seqbase + seqcap;
    }
    
    /* entire range already seen (or rotated past): signal retransmission */
    if (qfSeqCompare(bseq, sb->seqbase) < 0) return QF_SEQ_REXMIT;
    
    /* clip overlapping bottom of range */
    if (qfSeqCompare(aseq, sb->seqbase) < 0) aseq = sb->seqbase;

    /* set bits in the bitmap */
    brv = bimTestAndSetRange(&sb->map, (aseq - sb->seqbase)/sb->scale,
                                       (bseq - sb->seqbase)/sb->scale);
    
    /* translate intersection to retransmission detect */
    if (brv == BIM_PART || brv == BIM_FULL) {
        return QF_SEQ_REXMIT; // partial intersection -> retransmission
    } else if (brv == BIM_START || brv == BIM_EDGE) {
        return QF_SEQ_INORDER; // overlap at start or both sides -> inorder
    } else if (((aseq - sb->seqbase)/sb->scale) == 0) {
        return QF_SEQ_INORDER; // beginning of range -> presumed inorder
    } else if (bimTestBit(&sb->map, (aseq - sb->seqbase)/sb->scale) - 1) {
        return QF_SEQ_INORDER; // previous bit set -> inorder
    } else {
        return QF_SEQ_REORDER; // otherwise reordered
    }
}

void qfSeqBitsFinalizeLoss(qfSeqBits_t *sb)
{
    uint64_t bits;
    
    while ((bits = bimShiftDown(&sb->map))) {
        sb->lostseq_ct += ((bimHighBitSet(bits) - bimCountSet(bits)) * sb->scale);
        sb->seqbase += sb->scale * k64Bits;
    }
}

static void qfDynRexmit(qfDyn_t     *qd,
                        uint32_t    seq,
                        uint64_t    ms)
{
    /* increment retransmit counter */
    qd->rtx_ct++;
}

static void qfDynReorder(qfDyn_t     *qd,
                         uint32_t    seq,
                         uint64_t    ms)
{
    /* increment reorder counter */
    qd->reorder_ct++;
}


static int qfDynBreakFlight(qfDyn_t    *qd,
                             uint32_t   iat)
{
    if ((qd->seg_iat.n > kFlightMinSegments) &&
        (iat > (kFlightBreakIATDev * sstStdev(&qd->seg_iat))))
    {
        sstLinSmoothAdd(&qd->iatflight, qd->cur_iatflight);
        qd->cur_iatflight = 0;
        return 1;
    }
    return 0;
}

static uint32_t qfDynRttWalkEpbdp(qfDyn_t     *qd)
{
    /* simple flight-size based EPBDP */
    uint32_t flightsize = qd->iatflight.val;
    if (qd->cur_iatflight > flightsize) flightsize = qd->cur_iatflight;
    return flightsize;
}

static gboolean qfDynRttWalkSeq(qfDyn_t     *qd,
                         uint32_t    seq,
                         uint32_t    oct,
                         uint32_t    ms)
{
    /* short-circuit if we're looking for an ack */
    if ((qd->dynflags & QF_DYN_RTTW_STATE) == QF_DYN_RTTW_SA) return FALSE;

    /* try rttc measurement if we're looking for a seq */
    if ((qd->dynflags & QF_DYN_RTTW_STATE) == QF_DYN_RTTW_AS) {
        if (qfSeqCompare(seq, qd->rtt_next_san) >= 0) {
            /* found. update rttc. */
            qd->rttc = ms - qd->rtt_next_lms;
            if (qd->rttc && qd->rttm) {
                sstMeanAdd(&qd->rtt, qd->rttc + qd->rttm);
            }
        } else {
            /* not found. skip state switch. */
            return FALSE;
        }
    }
    
    /* updated rttc or in initial state; take seq and look for ack. */
    qd->dynflags = (qd->dynflags & ~QF_DYN_RTTW_AS) | QF_DYN_RTTW_SA;
    qd->rtt_next_lms = ms;
    qd->rtt_next_san = seq + oct;
    
    return TRUE;
}

static gboolean qfDynRttWalkAck  (qfDyn_t     *qd,
                              uint32_t    ack,
                              uint32_t    ms)
{
    /* short-circuit if we're not looking for an ack */
    if ((qd->dynflags & QF_DYN_RTTW_STATE) != QF_DYN_RTTW_SA) return FALSE;

    /* try rttm measurement */
    if (qfSeqCompare(ack, qd->rtt_next_san) >= 0) {
        /* yep, got the ack we're looking for */
        qd->rttm = ms - qd->rtt_next_lms;
        if (qd->rttc && qd->rttm) {
            sstMeanAdd(&qd->rtt, qd->rttc + qd->rttm);
        }
        
        /* set next expected seq */
        qd->dynflags = (qd->dynflags & ~QF_DYN_RTTW_SA) | QF_DYN_RTTW_AS;
        qd->rtt_next_lms = ms;
        qd->rtt_next_san = ack + qfDynRttWalkEpbdp(qd);
        return TRUE;
    }

    return FALSE;
}

void qfDynConfig(uint32_t bincap, uint32_t binscale) {
    qf_dyn_bincap = bincap;
    qf_dyn_binscale = binscale;
}

void qfDynFree(qfDyn_t      *qd)
{
    qfSeqBitsFree(&qd->sb);
}

void qfDynSyn(qfDyn_t     *qd,
              uint32_t    seq,
              uint32_t    ms)
{
    /* check for duplicate SYN */
    if (qd->dynflags & QF_DYN_SEQINIT) {
        // FIXME what to do here?
        return;
    }
    
    /* allocate structures 
       FIXME figure out how to delay this until we have enough packets */
    if (qf_dyn_bincap) {
        qfSeqBitsInit(&qd->sb, qf_dyn_bincap, qf_dyn_binscale);
    }
    
    /* set initial sequence number */
    qd->isn = qd->nsn = seq;
    qd->advlms = ms;
    
    /* note we've tracked the SYN */
    qd->dynflags |= QF_DYN_SEQINIT;
}

void qfDynSeq(qfDyn_t     *qd,
              uint32_t    seq,
              uint32_t    oct,
              uint32_t    ms)
{
    uint32_t              iat;
    qfSeqStat_t           seqstat;
    
    /* short circuit on no octets */
    if (!oct) {
        return;
    }
    
    if (!(qd->dynflags & QF_DYN_SEQINIT)) {
        return;
    }
    
    /* update MSS */
    if (oct > qd->mss) {
        qd->mss = oct;
    }
    
    /* track sequence numbers in binmap to detect order/loss */
    if (qfDynHasSeqbits(qd)) {
        seqstat = qfSeqBitsSegment(&(qd->sb), seq, seq + oct);
        if (seqstat == QF_SEQ_REXMIT) {
            qfDynRexmit(qd, seq, ms);
        } else if (seqstat == QF_SEQ_REORDER) {
            qfDynReorder(qd, seq, ms);
        }
    } else {
        seqstat = QF_SEQ_INORDER; // presume in order (need this for IAT)
    }
    
    /* advance sequence number if necessary */
    if (qfSeqCompare(seq, qd->nsn) > -1) {
        if (seq + oct < qd->nsn) ++(qd->wrap_ct);
        qd->nsn = seq + oct;          // next expected seq
        iat = ms - qd->advlms;        // calculate last IAT
        qd->advlms = ms;              // update time of advance

        /* update ack-based inflight */
        if (qd->dynflags & QF_DYN_ACKINIT) {
            sstMeanAdd(&qd->ack_inflight, qd->nsn - qd->fan);
        }
        
        /* detect iat flight break */
        sstMeanAdd(&qd->seg_iat, iat);
        if (seqstat == QF_SEQ_INORDER) qfDynBreakFlight(qd, iat);
        /* count octets in this flight */
        qd->cur_iatflight += oct;

        /* Do seq-side RTT calculation */
        qfDynRttWalkSeq(qd, seq, oct, ms);
        
        /* output situation after processing of segment to TMI if necessary */
#if QOF_DYN_TMI_ENABLE
        qfDynTmiDynamics(seq - qd->isn, qd->fan - qd->isn,
                         qd->rtt_next_san - qd->isn,
                         iat, qd->rttm, qd->rttc,
                         qd->rtx_ct, qd->reorder_ct);
#endif
        
    } else {        
        /* update max out of order */
        if ((qd->nsn - seq) > qd->reorder_max) {
            qd->reorder_max = qd->nsn - seq;
        }
    }
}

void qfDynAck(qfDyn_t     *qd,
              uint32_t    ack,
              uint32_t    ms)
{
    /* initialize if necessary */
    if (!(qd->dynflags & QF_DYN_ACKINIT)) {
        qd->dynflags |= QF_DYN_ACKINIT;        
        qd->fan = ack;
    } else if (qfSeqCompare(ack, qd->fan) > 0) {
        /* advance ack number */
        qd->fan = ack;
        
        /* Do ack-side RTT calculation */
        qfDynRttWalkAck(qd, ack, ms);
    }
}

void qfDynClose(qfDyn_t *qd) {
    // count loss
    if (qfDynHasSeqbits(qd)) {
        qfSeqBitsFinalizeLoss(&qd->sb);
    }
    // add last flight
    sstLinSmoothAdd(&qd->iatflight, qd->cur_iatflight);
}

uint64_t qfDynSequenceCount(qfDyn_t *qd, uint8_t flags) {
    uint64_t sc = qd->nsn - qd->isn + (k2e32 * qd->wrap_ct);
    if (sc) sc -= 1; // remove one: nsn is the next expected sequence number
    if (sc & (flags & YF_TF_SYN) && sc) sc -= 1; // remove one for the syn
    if (sc & (flags & YF_TF_FIN) && sc) sc -= 1; // remove one for the fin
    return sc;
}

void qfDynDumpStats() {
    if (qf_dyn_stat_ringover) {
        g_warning("%"PRIu64" RTT sample buffer overruns.",
                  qf_dyn_stat_ringover);
    }
}
