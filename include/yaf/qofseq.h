/**
 ** qofseq.h
 ** Sequence number tracking structures and prototypes for QoF
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


#ifndef _QOF_SEQ_H_
#define _QOF_SEQ_H_

#include <yaf/autoinc.h>

#define QF_SEQGAP_CT 8

typedef struct qfSeqGap_st {
    uint32_t        a;
    uint32_t        b;
} qfSeqGap_t;

typedef struct qfSeq_st {
    /* Gaps in seen sequence number space */
    qfSeqGap_t      gaps[QF_SEQGAP_CT];
    /* Initial sequence number */
    uint32_t        isn;
    /* Next sequence number expected */
    uint32_t        nsn;
    /* Wraparound count */
    uint32_t        wrapct;
} qfSeq_t;


