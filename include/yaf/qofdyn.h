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
#include <yaf/bitmap.h>
#include <yaf/streamstat.h>

/**
 * Compare sequence numbers A and B, accounting for 2e31 wraparound.
 *
 * @param a value to compare
 * @param b value to compare
 * @return >0 if a > b, 0 if a == b, <0 if a < b.
 */

int qfSeqCompare(uint32_t a, uint32_t b);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Sequence number bitmap structure
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
typedef struct qfSeqBits_st {
    bimBitmap_t     map;
    uint32_t        seqbase;
    uint32_t        scale;
    uint32_t        lostseq_ct;
} qfSeqBits_t;

typedef enum {
    QF_SEQ_INORDER,
    QF_SEQ_REORDER,
    QF_SEQ_REXMIT
} qfSeqStat_t;

void qfSeqBitsInit(qfSeqBits_t *sb, uint32_t capacity, uint32_t scale);

void qfSeqBitsFree(qfSeqBits_t *sb);

qfSeqStat_t qfSeqBitsSegment(qfSeqBits_t *sb, uint32_t aseq, uint32_t bseq);

void qfSeqBitsFinalizeLoss(qfSeqBits_t *sb);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * TCP dynamics structure
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define QF_DYN_SEQINIT          0x10000000 /* first sequence number seen */
#define QF_DYN_ACKINIT          0x20000000 /* first ack seen */
#define QF_DYN_RTTW_SA          0x40000000 /* rttwalk looking for seq-ack */
#define QF_DYN_RTTW_AS          0x80000000 /* rttwalk looking for ack-seq */
#define QF_DYN_RTTW_STATE       0xC0000000 /* rttwalk state mask */
#define QF_DYN_EXPORT_FLAGS     0x0FFFFFFF /* feature flags mask */
#define QF_DYN_ECT0             0x00000001 /* observed an ECT(0) codepoint */
#define QF_DYN_ECT1             0x00000002 /* observed an ECT(1) codepoint */
#define QF_DYN_CE               0x00000004 /* observed a CE codepoint */
#define QF_DYN_TS               0x00000010 /* observed a TS option */
#define QF_DYN_SACK             0x00000020 /* observed a SACK option */
#define QF_DYN_WS               0x00000040 /* observed a window scale option */

typedef struct qfDyn_st {
    /** Bitmap for storing seen sequence numbers */
    qfSeqBits_t     sb;
    /* Non-empty segment interarrival time tracking */
    sstMean_t       seg_iat;
    /* Smoothed IAT flight size series */
    sstLinSmooth_t  iatflight;
    /* Current IAT flight size */
    uint32_t        cur_iatflight;
    /* Mean/min/max RTT */
    sstMean_t       rtt;
    /* Next ack/tsecr expected */
    uint32_t        rtt_next_tsack;
    /* Time of tsval or ack ( + rttx = ctime) */
    uint32_t        rtt_next_lms;
    /* observed forward RTT (rtt measured) */
    uint32_t        rttm;
    /* observed reverse RTT (rtt correction term) */
    uint32_t        rttc;
    /* Initial sequence number */
    uint32_t        isn;
    /* Next sequence number expected */
    uint32_t        nsn;
    /* Timestamp of last sequence number advance */
    uint32_t        advlms;
    /* Final acknowledgment number observed */
    uint32_t        fan;
    /* Sequence number space wraparound count */
    uint32_t        wrap_ct;
    /* Detected retransmitted segment count */
    uint32_t        rtx_ct;
    /* Detected retransmitted segment burst count */
    uint32_t        rtx_burst_ct;
    /* Last burst start time */
    uint32_t        rtx_burst_lms;
    /* Detected reordered segment count */
    uint32_t        ooo_ct;
    /* Maxumum observed reordering (nsn - seq) */
    uint32_t        ooo_max;
    /* Observed maximum segment size */
    uint16_t        mss;
    /* Declared (via tcp option) maximum segment size */
    uint16_t        mss_opt;
    /* Dynamics control and feature presence flags */
    uint32_t        dynflags;
} qfDyn_t;

void qfDynClose(qfDyn_t   *qd);

void qfDynFree(qfDyn_t    *qd);

void qfDynSyn(qfDyn_t     *qd,
              uint32_t    seq,
              uint32_t    ms);

void qfDynSeq(qfDyn_t     *qd,
              uint32_t    seq,
              uint32_t    oct,
              uint32_t    tsval,
              uint32_t    tsecr,
              uint32_t    ms);

void qfDynAck(qfDyn_t     *qd,
              uint32_t    ack,
              uint32_t    sack,
              uint32_t    tsval,
              uint32_t    tsecr,
              uint32_t    ms);

void qfDynEcn(qfDyn_t *qd,
              uint8_t tosbyte);

void qfDynTcpOpts(qfDyn_t   *qd,
                  gboolean  ts_present,
                  gboolean  ws_present,
                  gboolean  sack_present);


void qfDynConfig(gboolean enable,
                 gboolean enable_rtt,
                 gboolean enable_rtx,
                 uint32_t span,
                 uint32_t scale);

uint64_t qfDynSequenceCount(qfDyn_t *qd, uint8_t flags);

void qfDynDumpStats();

#endif /* idem */
