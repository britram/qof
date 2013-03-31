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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Low level sequence number - timestamp sampling structure 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct qfSeqTime_st;
typedef struct qfSeqTime_st qfSeqTime_t;

/** Ring of sequence number / time tuples */
typedef struct qfSeqRing_st {
    qfSeqTime_t     *bin;
    size_t          bincount;
    size_t          opcount;
    size_t          overcount;
    size_t          head;
    size_t          tail;
} qfSeqRing_t;

void qfSeqRingInit(qfSeqRing_t              *sr,
                   size_t                   capacity);

void qfSeqRingFree(qfSeqRing_t              *sr);

void qfSeqRingAddSample(qfSeqRing_t         *sr,
                        uint32_t            seq,
                        uint32_t            ms);

uint32_t qfSeqRingRTT(qfSeqRing_t           *sr,
                      uint32_t              ack,
                      uint32_t              ms);


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * TCP dynamics structure
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define QF_DYN_SEQINIT      0x00000001 /* first sequence number seen */
#define QF_DYN_ACKINIT      0x00000002 /* first ack seen */
#define QF_DYN_SEQADV       0x00000004 /* SEQ advanced on last operation */
#define QF_DYN_RTTCORR      0x00000010 /* ACK advanced, update rtt_corr */
#define QF_DYN_RTTVALID     0x00000020 /* we think rtt is usable */

typedef struct qfDyn_st {
//    qfSeqBin_t      sb;
    qfSeqRing_t     sr;
    uint16_t        sr_skip;
    uint16_t        sr_period;
    uint32_t        isn;
    uint32_t        fsn;
    uint32_t        fan;
    uint32_t        fanlms;
    uint32_t        wrap_ct;
//    uint32_t        rtx_ct;
    uint32_t        inflight_max;
    uint32_t        reorder_max;    
    uint32_t        rtt_est;
    uint32_t        rtt_min;
    uint32_t        rtt_corr;
    uint32_t        mss;
    uint32_t        dynflags;
} qfDyn_t;

void qfDynFree(qfDyn_t      *qd);

void qfDynSeq(qfDyn_t     *qd,
              uint32_t    seq,
              uint32_t    oct,
              uint32_t    ms);

void qfDynAck(qfDyn_t     *qd,
              uint32_t    ack,
              uint32_t    ms);

void qfDynSetParams(size_t bincap,
                    size_t binscale,
                    size_t ringcap);

uint64_t qfDynSequenceCount(qfDyn_t *qd, uint8_t flags);

#endif /* idem */
