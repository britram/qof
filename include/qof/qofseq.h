/**
 ** qofseq.h
 ** Sequence number tracking structures and prototypes for QoF
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


#ifndef _QOF_SEQ_H_
#define _QOF_SEQ_H_

#include <qof/autoinc.h>
#include <qof/streamstat.h>
#include <qof/qofrtt.h>

#ifndef QF_SEQGAP_CT
#define QF_SEQGAP_CT 8
#endif

typedef struct qfSeqGap_st {
    uint32_t        a;
    uint32_t        b;
} qfSeqGap_t;

typedef struct qfSeq_st {
    /** Gaps in seen sequence number space */
    qfSeqGap_t      gaps[QF_SEQGAP_CT];
    /* Non-empty segment interarrival time tracking */
    sstMean_t       seg_iat;
    /* Non-empty segment IAT/IDT variance tracking */
    sstMean_t       seg_variat;
    /** Initial advance time */
    uint32_t        initlms;
    /** Initial timestamp value */
    uint32_t        initsval;
    /** Time of last sequence number advance */
    uint32_t        advlms;
    /** Timestamp at last advance */
    uint32_t        advtsval;
    /** Initial sequence number */
    uint32_t        isn;
    /** Next sequence number expected */
    uint32_t        nsn;
    /** sequence wrap counter */
    uint32_t        wrapct;
    /** low bits ms wrap counter */
    uint32_t         lmswrap;
    /** Timestamp wrap counter */
    uint32_t         tsvalwrap;
    /** Retransmitted segment count */
    uint32_t        rtx;
    /** Segment reorder count */
    uint32_t        ooo;
    /** Maximum sequence inversion */
    uint32_t        maxooo;
    /** Sequence loss count */
    uint32_t        seqlost;
    /** Burst loss count */
    uint32_t        lossct;
    /** Burst loss last start */
    uint32_t        losslms;
} qfSeq_t;

#endif /* idem */

void qfSeqFirstSegment(qfSeq_t *qs,
                       uint8_t flags,
                       uint32_t seq,
                       uint32_t oct,
                       uint32_t ms,
                       uint32_t tsval,
                       gboolean do_ts);

int qfSeqSegment(qfSeq_t *qs, qfRtt_t *rtt, uint16_t mss,
                 uint8_t flags, uint32_t seq, uint32_t oct,
                 uint32_t ms, uint32_t tsval,
                 gboolean do_ts, gboolean do_iat);

uint64_t qfSeqCount(qfSeq_t *qs, uint8_t flags);

uint32_t qfSeqCountLost(qfSeq_t *qs);

uint32_t qfTimestampHz(qfSeq_t *qs);

void qfSeqContinue(qfSeq_t *cont_seq, qfSeq_t *orig_seq);
