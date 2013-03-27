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

#ifndef _QOF_DYN_H_
#define _QOF_DYN_H_

#include <yaf/autoinc.h>
#include <yaf/yaftab.h>

typedef struct qfSeqBin_st {
    uint64_t    *bin;
    size_t      bincount;
    size_t      opcount;
    size_t      scale;
    size_t      binbase;
    uint32_t    seqbase;
} qfSeqBin_t;

typedef enum qfSeqBinRes_en {
    QF_SEQBIN_NO_ISECT,
    QF_SEQBIN_PART_ISECT,
    QF_SEQBIN_FULL_ISECT,
    QF_SEQBIN_OUT_OF_RANGE
} qfSeqBinRes_t;

typedef struct qfSeqTime_st {
    uint32_t    lms;
    uint32_t    seq;
} qfSeqTime_t;

int qfSeqCompare(uint32_t a, uint32_t b);

void qfSeqBinInit(qfSeqBin_t     *sb,
                  size_t         capacity,
                  size_t         scale);

void qfSeqBinFree(qfSeqBin_t *sb);

qfSeqBinRes_t qfSeqBinTestAndSet(qfSeqBin_t      *sb,
                                 uint32_t        aseq,
                                 uint32_t        bseq);

#endif /* idem */
