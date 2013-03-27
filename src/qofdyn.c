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
#include "qofdyn.h"

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

int qfSeqCompare(uint32_t a, uint32_t b) {
    return a == b ? 0 : ((a - b) & 0x80000000) ? -1 : 1;
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
    yg_slice_free1(sb->bincount * sizeof(uint64_t), sb->bin);
}


static void qfSeqBinShiftup(qfSeqBin_t     *sb)
{
    while (sb->bin[sb->binbase] == UINT64_MAX) {
        sb->binbase++;
        if (sb->binbase >= sb->bincount) sb->binbase = 0;
        sb->seqbase += sb->scale * sizeof(uint64_t) * 8;
    }
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

static qfSeqBinRes_t qfSeqBinTestMask(uint64_t bin, uint64_t mask) {
    if ((bin & mask) == mask) {
        return QF_SEQBIN_FULL_ISECT;
    } else if ((bin & mask) == 0) {
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
    size_t i, j;
    uint64_t m;
    uint32_t maxseq;
    
    qfSeqBinRes_t res;
    
    /* initialize seqbase if first test and set */
    if (!(sb->opcount++)) {
        /* initialize seqbase if first test and set */
        sb->seqbase = (aseq / sb->scale) * sb->scale;
    } else {
        /* shift up to compress space */
        qfSeqBinShiftup(sb);
    }
    
    /* check range: things above the bin range are out of range */
    maxseq = sb->seqbase + sb->bincount * sizeof(uint64_t) * 8 * sb->scale;
    if (qfSeqCompare(aseq, maxseq) > 0) return QF_SEQBIN_OUT_OF_RANGE;
    if (qfSeqCompare(aseq, maxseq) > 0) return QF_SEQBIN_OUT_OF_RANGE;
    
    /* check range: things below the bin range are already fully intersected */
    if (qfSeqCompare(bseq, sb->seqbase) < 0) return QF_SEQBIN_FULL_ISECT;

    /* check range: clip overlapping bottom of range */
    if (qfSeqCompare(aseq, sb->seqbase) < 0) aseq = sb->seqbase;
    
    /* get bin numbers */
    i = qfSeqBinNum(sb, aseq);
    j = qfSeqBinNum(sb, bseq);
    
    /* special case: bins are the same, intersect the masks, shortcircuit */
    if (i == j) {
        m = startmask64[qfSeqBinBit(sb, aseq)] &
            endmask64[qfSeqBinBit(sb, bseq)];
        res = qfSeqBinTestMask(sb->bin[i], m);
        sb->bin[i] |= m;
        return res;
    }

    /* handle first bin */
    m = startmask64[qfSeqBinBit(sb, aseq)];
    res = qfSeqBinTestMask(sb->bin[i], m);
    sb->bin[i] |= m;
    
    /* handle middle bins */
    i++; if (i >= sb->bincount) i = 0;
    while (i != j) {
        res = qfSeqBinResCombine(res, qfSeqBinTestMask(sb->bin[i], UINT64_MAX));
        sb->bin[i] = UINT64_MAX;
        i++; if (i >= sb->bincount) i = 0;
    }
    
    /* handle last bin */
    m = endmask64[qfSeqBinBit(sb, aseq)];
    res = qfSeqBinTestMask(sb->bin[i], m);
    sb->bin[i] |= m;
    
    return res;
}

