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

#define QF_DYN_DEBUG 1

static const uint64_t startmask64[] = {
    0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFEULL,
    0xFFFFFFFFFFFFFFFCULL, 0xFFFFFFFFFFFFFFF8ULL,
    0xFFFFFFFFFFFFFFF0ULL, 0xFFFFFFFFFFFFFFE0ULL,
    0xFFFFFFFFFFFFFFC0ULL, 0xFFFFFFFFFFFFFF80ULL,
    0xFFFFFFFFFFFFFF00ULL, 0xFFFFFFFFFFFFFE00ULL,
    0xFFFFFFFFFFFFFC00ULL, 0xFFFFFFFFFFFFF800ULL,
    0xFFFFFFFFFFFFF000ULL, 0xFFFFFFFFFFFFE000ULL,
    0xFFFFFFFFFFFFC000ULL, 0xFFFFFFFFFFFF8000ULL,
    0xFFFFFFFFFFFF0000ULL, 0xFFFFFFFFFFFE0000ULL,
    0xFFFFFFFFFFFC0000ULL, 0xFFFFFFFFFFF80000ULL,
    0xFFFFFFFFFFF00000ULL, 0xFFFFFFFFFFE00000ULL,
    0xFFFFFFFFFFC00000ULL, 0xFFFFFFFFFF800000ULL,
    0xFFFFFFFFFF000000ULL, 0xFFFFFFFFFE000000ULL,
    0xFFFFFFFFFC000000ULL, 0xFFFFFFFFF8000000ULL,
    0xFFFFFFFFF0000000ULL, 0xFFFFFFFFE0000000ULL,
    0xFFFFFFFFC0000000ULL, 0xFFFFFFFF80000000ULL,
    0xFFFFFFFF00000000ULL, 0xFFFFFFFE00000000ULL,
    0xFFFFFFFC00000000ULL, 0xFFFFFFF800000000ULL,
    0xFFFFFFF000000000ULL, 0xFFFFFFE000000000ULL,
    0xFFFFFFC000000000ULL, 0xFFFFFF8000000000ULL,
    0xFFFFFF0000000000ULL, 0xFFFFFE0000000000ULL,
    0xFFFFFC0000000000ULL, 0xFFFFF80000000000ULL,
    0xFFFFF00000000000ULL, 0xFFFFE00000000000ULL,
    0xFFFFC00000000000ULL, 0xFFFF800000000000ULL,
    0xFFFF000000000000ULL, 0xFFFE000000000000ULL,
    0xFFFC000000000000ULL, 0xFFF8000000000000ULL,
    0xFFF0000000000000ULL, 0xFFE0000000000000ULL,
    0xFFC0000000000000ULL, 0xFF80000000000000ULL,
    0xFF00000000000000ULL, 0xFE00000000000000ULL,
    0xFC00000000000000ULL, 0xF800000000000000ULL,
    0xF000000000000000ULL, 0xE000000000000000ULL,
    0xC000000000000000ULL, 0x8000000000000000ULL,
};

static const uint64_t endmask64[] = {
                           0x0000000000000000ULL,
    0x0000000000000001ULL, 0x0000000000000003ULL,
    0x0000000000000007ULL, 0x000000000000000FULL,
    0x000000000000001FULL, 0x000000000000003FULL,
    0x000000000000007FULL, 0x00000000000000FFULL,
    0x00000000000001FFULL, 0x00000000000003FFULL,
    0x00000000000007FFULL, 0x0000000000000FFFULL,
    0x0000000000001FFFULL, 0x0000000000003FFFULL,
    0x0000000000007FFFULL, 0x000000000000FFFFULL,
    0x000000000001FFFFULL, 0x000000000003FFFFULL,
    0x000000000007FFFFULL, 0x00000000000FFFFFULL,
    0x00000000001FFFFFULL, 0x00000000003FFFFFULL,
    0x00000000007FFFFFULL, 0x0000000000FFFFFFULL,
    0x0000000001FFFFFFULL, 0x0000000003FFFFFFULL,
    0x0000000007FFFFFFULL, 0x000000000FFFFFFFULL,
    0x000000001FFFFFFFULL, 0x000000003FFFFFFFULL,
    0x000000007FFFFFFFULL, 0x00000000FFFFFFFFULL,
    0x00000001FFFFFFFFULL, 0x00000003FFFFFFFFULL,
    0x00000007FFFFFFFFULL, 0x0000000FFFFFFFFFULL,
    0x0000001FFFFFFFFFULL, 0x0000003FFFFFFFFFULL,
    0x0000007FFFFFFFFFULL, 0x000000FFFFFFFFFFULL,
    0x000001FFFFFFFFFFULL, 0x000003FFFFFFFFFFULL,
    0x000007FFFFFFFFFFULL, 0x00000FFFFFFFFFFFULL,
    0x00001FFFFFFFFFFFULL, 0x00003FFFFFFFFFFFULL,
    0x00007FFFFFFFFFFFULL, 0x0000FFFFFFFFFFFFULL,
    0x0001FFFFFFFFFFFFULL, 0x0003FFFFFFFFFFFFULL,
    0x0007FFFFFFFFFFFFULL, 0x000FFFFFFFFFFFFFULL,
    0x001FFFFFFFFFFFFFULL, 0x003FFFFFFFFFFFFFULL,
    0x007FFFFFFFFFFFFFULL, 0x00FFFFFFFFFFFFFFULL,
    0x01FFFFFFFFFFFFFFULL, 0x03FFFFFFFFFFFFFFULL,
    0x07FFFFFFFFFFFFFFULL, 0x0FFFFFFFFFFFFFFFULL,
    0x1FFFFFFFFFFFFFFFULL, 0x3FFFFFFFFFFFFFFFULL, 
    0x7FFFFFFFFFFFFFFFULL
};

/** Constant - 2^32 (for sequence number calculations) */
static const uint64_t k2e32 = 0x100000000ULL;

static const unsigned int kAlpha = 8;
static const unsigned int kSeqSamplePeriodMs = 1;

static size_t qf_dyn_bincap = 0;
static size_t qf_dyn_binscale = 0;
static size_t qf_dyn_ringcap = 0;

int qfSeqCompare(uint32_t a, uint32_t b) {
    return a == b ? 0 : ((a - b) & 0x80000000) ? -1 : 1;
}

static unsigned int qfCountBits(uint64_t v) {
    static const uint64_t mask[] = {
        0x5555555555555555ULL,
        0x3333333333333333ULL,
        0x0F0F0F0F0F0F0F0FULL,
        0x00FF00FF00FF00FFULL,
        0x0000FFFF0000FFFFULL,
        0x00000000FFFFFFFFULL
    };
    
    static const unsigned int shift[] = { 1, 2, 4, 8, 16, 32 };
    
    uint64_t c;

    c = v - ((v >> shift[0]) & mask[0]);
    c = ((c >> shift[1]) & mask[1]) + (c & mask[1]);
    c = ((c >> shift[2]) + c) & mask[2];
    c = ((c >> shift[3]) + c) & mask[3];
    c = ((c >> shift[4]) + c) & mask[4];
    return (unsigned int)((c >> shift[5]) + c) & mask[5];
}

void qfSeqBinInit(qfSeqBin_t     *sb,
                  size_t         capacity,
                  size_t         scale)
{
    /* zero structure */
    memset(sb, 0, sizeof(*sb));
    
    /* allocate bin array */
    sb->scale = scale;
    sb->bincount = capacity / scale / (sizeof(uint64_t) * 8);
    sb->bin = yg_slice_alloc0(sb->bincount * sizeof(uint64_t));
}

void qfSeqBinFree(qfSeqBin_t *sb) {
    if (sb->bin) yg_slice_free1(sb->bincount * sizeof(uint64_t), sb->bin);
}


static void qfSeqBinShiftComplete(qfSeqBin_t     *sb)
{
    while (sb->bin[sb->binbase] == UINT64_MAX) {
        sb->bin[sb->binbase] = 0;
        sb->binbase++;
        if (sb->binbase >= sb->bincount) sb->binbase = 0;
        sb->seqbase += sb->scale * sizeof(uint64_t) * 8;
    }
}

static size_t qfSeqBinShiftToSeq(qfSeqBin_t *sb, uint32_t seq)
{
    unsigned int bits = 0;

    while (sb->seqbase < seq) {
        bits += (8 * sizeof(uint64_t)) - qfCountBits(sb->bin[sb->binbase]);
        sb->bin[sb->binbase] = 0;
        sb->binbase++;
        if (sb->binbase >= sb->bincount) sb->binbase = 0;
        sb->seqbase += sb->scale * sizeof(uint64_t) * 8;
    }
    
    return bits * sb->scale;
}

static size_t qfSeqBinNum(qfSeqBin_t   *sb,
                           uint32_t     seq)
{
    return         ((seq - sb->seqbase) /
            (sizeof(uint64_t) * 8 * sb->scale)) % sb->bincount;
}

static size_t qfSeqBinBit(qfSeqBin_t    *sb,
                          uint32_t      seq)
{
    return ((seq - sb->seqbase) / sb->scale) % (sizeof(uint64_t) * 8);
}

static qfSeqBinRes_t qfSeqBinTestMask(uint64_t bin, uint64_t omask, uint64_t imask) {
    if ((bin & omask) == omask) {
        return QF_SEQBIN_FULL_ISECT;
    } else if ((bin & imask) == 0)  {
        return QF_SEQBIN_NO_ISECT;
    } else {
        return QF_SEQBIN_PART_ISECT;
    }
}

static qfSeqBinRes_t qfSeqBinResCombine(qfSeqBinRes_t a, qfSeqBinRes_t b)
{
    if (a == b)
        return a;
    else
        return QF_SEQBIN_PART_ISECT;
}

qfSeqBinRes_t qfSeqBinTestAndSet(qfSeqBin_t      *sb,
                                 uint32_t        aseq,
                                 uint32_t        bseq)
{
    size_t i, j, abit, bbit;
    uint64_t omask, imask;
    uint32_t maxseq, seqcap;
    
    qfSeqBinRes_t res;
    
    /* initialize seqbase if first test and set */
    if (!(sb->opcount++)) {
        /* initialize seqbase if first test and set */
        sb->seqbase = (aseq / sb->scale) * sb->scale;
    } else {
        /* shift up to compress space */
        qfSeqBinShiftComplete(sb);
    }
    
    /* check range: force range forward and count, on out of range */
    seqcap = sb->bincount * sizeof(uint64_t) * 8 * sb->scale;
    maxseq = sb->seqbase + seqcap;
    if (qfSeqCompare(bseq, maxseq) > 0) {
        sb->lostseq_ct += qfSeqBinShiftToSeq(sb, 1 + bseq - seqcap);
    }
    
    /* check range: things below the bin range are already fully intersected */
    if (qfSeqCompare(bseq, sb->seqbase) < 0) return QF_SEQBIN_FULL_ISECT;

    /* check range: clip overlapping bottom of range */
    if (qfSeqCompare(aseq, sb->seqbase) < 0) aseq = sb->seqbase;
    /* get bin numbers */
    i = qfSeqBinNum(sb, aseq);
    j = qfSeqBinNum(sb, bseq);
    
    /* special case: bins are the same, intersect the masks, shortcircuit */
    if (i == j) {
        abit = qfSeqBinBit(sb, aseq);
        bbit = qfSeqBinBit(sb, bseq);
        omask = startmask64[abit] & endmask64[bbit];
        imask = startmask64[abit + 1] & endmask64[bbit - 1]; // FIXME unsafe
        res = qfSeqBinTestMask(sb->bin[i], omask, imask);
        sb->bin[i] |= omask;
        return res;
    }

    /* handle first bin */
    abit = qfSeqBinBit(sb, aseq);
    omask = startmask64[abit];
    imask = startmask64[abit+1];
    res = qfSeqBinTestMask(sb->bin[i], omask, imask);
    sb->bin[i] |= omask;
    
    /* handle middle bins */
    i++; if (i >= sb->bincount) i = 0;
    while (i != j) {
        res = qfSeqBinResCombine(res,
              qfSeqBinTestMask(sb->bin[i], UINT64_MAX, UINT64_MAX));
        sb->bin[i] = UINT64_MAX;
        i++; if (i >= sb->bincount) i = 0;
    }
    
    /* handle last bin */
    bbit = qfSeqBinBit(sb, aseq);
    omask = endmask64[bbit];
    imask = endmask64[bbit-1];
    res = qfSeqBinTestMask(sb->bin[i], omask, imask);
    sb->bin[i] |= omask;
    
    return res;
}

/** Sequence number / time tuple */
struct qfSeqTime_st {
    uint32_t    ms;
    uint32_t    seq;
};

void qfSeqRingInit(qfSeqRing_t              *sr,
                   size_t                   capacity)
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
        sr->overcount++;
#if QF_DYN_DEBUG
        fprintf(stderr, "rtts-over (%u) ", sr->overcount );
#endif
    }
    
    /* insert sample */
    sr->bin[sr->head].seq = seq;
    sr->bin[sr->head].ms = ms;
    sr->opcount++;
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

#if QF_DYN_DEBUG
    fprintf(stderr, "(seq %u @ %u ack %u @ %u) ",
            sr->bin[sr->tail].seq,
            sr->bin[sr->tail].ms,
            ack, ms);
#endif
    
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

static int qfDynHasSeqbin(qfDyn_t *qd) { return qd->sb.bin != 0; }

static int qfDynHasSeqring(qfDyn_t *qd) { return qd->sr.bin != 0; }

static int qfDynSeqSampleP(qfDyn_t     *qd,
                           uint64_t     ms)
{
    /* don't sample without seqring */
    if (!qfDynHasSeqring(qd)) {
#if QF_DYN_DEBUG
        fprintf(stderr, "nortts-nil ");
#endif
        return 0;
    }
    
    /* don't sample higher than sample rate */
    if ((qd->sr.bin[qd->sr.head].ms + kSeqSamplePeriodMs) > ms)
    {
#if QF_DYN_DEBUG
        fprintf(stderr, "nortts-rate ");
#endif        
        return 0;
    }
    
    /* don't sample until we've waited out the sample period */
    if (qd->sr_skip < qd->sr_period) {
#if QF_DYN_DEBUG
        fprintf(stderr, "nortts-skip (%u of %u)", qd->sr_skip, qd->sr_period);
#endif
        qd->sr_skip++;
        return 0;
    }
    
    /* good to sample, calculate next period */
    if ((qd->dynflags & QF_DYN_SEQINIT) && (qd->dynflags & QF_DYN_ACKINIT)) {
        qd->sr_period = ((qd->fsn - qd->fan) / qd->mss) / qfSeqRingAvail(&qd->sr);
        if (qd->sr_period) qd->sr_period--;        
    } else {
        qd->sr_period = 0;
    }
    qd->sr_skip = 0;
    
    return 1;
}

static void qfDynRexmit(qfDyn_t     *qd,
                        uint32_t    seq,
                        uint64_t    ms)
{
    /* increment retransmit counter */
    qd->rtx_ct++;
}

static void qfDynCorrectRTT(qfDyn_t   *qd,
                           uint32_t    crtt)
{
    if (!qd->rtt_corr || (crtt < qd->rtt_corr)) qd->rtt_corr = crtt;
}

static void qfDynTrackRTT(qfDyn_t   *qd,
                          uint32_t  irtt)
{
    /* add correction factor to raw sample */
    irtt += qd->rtt_corr;
    
    if (qd->rtt_est) {
        /* moving average */
        qd->rtt_est = (qd->rtt_est * (kAlpha - 1) + irtt) / kAlpha;
    } else {
        /* take initial sample */
        qd->rtt_est = irtt;
    }

    /* minimize only if we have a correction term */
    if (qd->rtt_corr &&
        (!qd->rtt_min || (qd->rtt_est < qd->rtt_min)))
    {
        qd->rtt_min = qd->rtt_est;
        qd->dynflags = QF_DYN_RTTVALID; // FIXME might want to be less eager here
    }
}

void qfDynSetParams(size_t bincap, size_t binscale, size_t ringcap) {
    qf_dyn_bincap = bincap;
    qf_dyn_binscale = binscale;
    qf_dyn_ringcap = ringcap;
}

void qfDynFree(qfDyn_t      *qd)
{
    qfSeqBinFree(&qd->sb);
    qfSeqRingFree(&qd->sr);
}

void qfDynSeq(qfDyn_t     *qd,
              uint32_t    seq,
              uint32_t    oct,
              uint32_t    ms)
{
    qfSeqBinRes_t   sbres;

#if QF_DYN_DEBUG
    fprintf(stderr, "qd ts %u seq [%10u - %10u] ", (uint32_t) (ms & UINT32_MAX), seq - oct, seq);
#endif
 
    /* short circuit on no octets */
    if (!oct) {
#if QF_DYN_DEBUG
        fprintf(stderr, "empty\n");
#endif
        return;
    }
    
    /* initialise if not yet done */
    if (!qd->dynflags & QF_DYN_SEQINIT) {        
        /* allocate structures */
        if (qf_dyn_ringcap) {
            qfSeqRingInit(&qd->sr, qf_dyn_ringcap);
        }
        if (qf_dyn_bincap) {
            qfSeqBinInit(&qd->sb, qf_dyn_bincap, qf_dyn_binscale);
        }
        
        /* set ISN */
        qd->isn = seq - oct;
        
        /* force sequence number advance */
        qd->dynflags |= QF_DYN_SEQINIT | QF_DYN_SEQADV;
#if QF_DYN_DEBUG
        fprintf(stderr, "init ");
#endif        
    }
    
    /* update MSS */
    if (oct > qd->mss) {
        qd->mss = oct;
#if QF_DYN_DEBUG
        fprintf(stderr, "mss %u ", oct);
#endif
    }
    
    /* update and minimize RTT correction factor if necessary */
    if ((qd->dynflags & QF_DYN_RTTCORR) && (seq >= qd->fan)) {
        qd->dynflags &= ~QF_DYN_RTTCORR;
        qfDynCorrectRTT(qd, ms - qd->fanlms);
#if QF_DYN_DEBUG
        fprintf(stderr, "rttc %u ", qd->rtt_corr);
#endif
    }
   
    /* track sequence numbers in binmap to detect order/loss */
    if (qfDynHasSeqbin(qd)) {
        sbres = qfSeqBinTestAndSet(&(qd->sb), seq - oct, seq);
        if (sbres != QF_SEQBIN_NO_ISECT) {
            qfDynRexmit(qd, seq, ms);
#if QF_DYN_DEBUG
            fprintf(stderr, "rexmit ");
#endif
        }
    }
    
    /* advance sequence number if necessary */
    if (qd->dynflags & QF_DYN_SEQADV || qfSeqCompare(seq, qd->fsn) > 0) {
        qd->dynflags |= QF_DYN_SEQADV;
        if (seq < qd->fsn) qd->wrap_ct++;
        qd->fsn = seq;
#if QF_DYN_DEBUG
        fprintf(stderr, "adv ");
#endif
        
        /* update max inflight */
        if (qd->inflight_max > (qd->fsn - qd->fan)) {
            qd->inflight_max = (qd->fsn - qd->fan);
#if QF_DYN_DEBUG
            fprintf(stderr, "ifmax %u ", qd->inflight_max);
#endif
        }
        
        /* take time sample, if necessary */
        if (qfDynSeqSampleP(qd, ms)) {
            qfSeqRingAddSample(&(qd->sr), seq, ms);
#if QF_DYN_DEBUG
            fprintf(stderr, "rtts ");
#endif
        }
        
    } else {
        qd->dynflags &= ~QF_DYN_SEQADV;
        
        /* update max out of order */
        if ((qd->fsn - seq) > qd->reorder_max) {
            qd->reorder_max = qd->fsn - seq;
#if QF_DYN_DEBUG
            fprintf(stderr, "remax %u ", qd->reorder_max);
#endif
        }
    }
#if QF_DYN_DEBUG
    fprintf(stderr, "\n");
#endif
}

void qfDynAck(qfDyn_t     *qd,
              uint32_t    ack,
              uint32_t    ms)
{
    uint32_t irtt;
    
    /* advance ack number if necessary */
    if (!(qd->dynflags & QF_DYN_ACKINIT) || (qfSeqCompare(ack, qd->fan) > 0)) {
        qd->dynflags |= QF_DYN_ACKINIT;
        qd->fan = ack;
        qd->fanlms = ms;
 
#if QF_DYN_DEBUG
        fprintf(stderr, "qd ts %u ack %10u ", ms, ack);
#endif
        
        /* sample RTT */
        if (qfDynHasSeqring(qd)) {
            irtt = qfSeqRingRTT(&(qd->sr), ack, ms);
            if (irtt) {
                qfDynTrackRTT(qd, irtt);
#if QF_DYN_DEBUG
                fprintf(stderr, "irtt %u ", irtt);
#endif
            }
            qd->dynflags |= QF_DYN_RTTCORR;
        }        
#if QF_DYN_DEBUG
        fprintf(stderr, "\n");
#endif
    }
}

uint64_t qfDynSequenceCount(qfDyn_t *qd, uint8_t flags) {
    uint64_t sc = qd->fsn - qd->isn + (k2e32 * qd->wrap_ct);
    if ((flags & YF_TF_FIN) && sc) sc -= 1;
    return sc;
}
