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
#include <yaf/streamstat.h>

#define QF_SEQGAP_CT 8

typedef struct qfSeqGap_st {
    uint32_t        a;
    uint32_t        b;
} qfSeqGap_t;

typedef struct qfSeq_st {
    /** Gaps in seen sequence number space */
    qfSeqGap_t      gaps[QF_SEQGAP_CT];
    /* Non-empty segment interarrival time tracking */
    sstMean_t       seg_iat;
    /** Time of last sequence number advance */
    uint32_t        advlms;
    /** Initial sequence number */
    uint32_t        isn;
    /** Next sequence number expected */
    uint32_t        nsn;
    /** Wraparound count */
    uint32_t        wrapct;
    /** Retransmitted segment count */
    uint32_t        rtx;
    /** Segment reorder count */
    uint32_t        ooo;
    /** Maximum sequence inversion */
    uint32_t        maxooo;
    /** Sequence loss count */
    uint32_t        seqlost;
} qfSeq_t;

#endif /* idem */

void qfSeqFirstSegment(qfSeq_t *qs,
                       uint8_t flags,
                       uint32_t seq,
                       uint32_t oct,
                       uint32_t ms);

int qfSeqSegment(qfSeq_t *qs,
                  uint8_t flags,
                  uint32_t seq,
                  uint32_t oct,
                  uint32_t ms,
                  gboolean do_iat);

uint64_t qfSeqCount(qfSeq_t *qs, uint8_t flags);

uint32_t qfSeqCountLost(qfSeq_t *qs);
