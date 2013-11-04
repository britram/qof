/**
 ** qofrtt.h
 ** RTT estimation data structures and function prototypes for QoF
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

#ifndef _QOF_RTT_H_
#define _QOF_RTT_H_

#include <qof/autoinc.h>
#include <qof/streamstat.h>

typedef struct qfRttDir_st {
    /** Next ack/tsecr expected in this direction */
    uint32_t    tsack;
    /** Time seq/tsval seen ( + rttx = ctime) in this direction */
    uint32_t    lms;
    /** True if waiting for ACK */
    uint32_t    ackwait : 1;
    /** True if waiting for ECR */
    uint32_t    ecrwait : 1;
    /** Last observation (in milliseconds) */
    uint32_t    obs_ms  : 30;
} qfRttDir_t;

/** per-biflow RTT tracking structure */
typedef struct qfRtt_st {
    /** smoothed RTT estimate */
    sstLinSmooth_t   val;
    /** Forward observations */
    qfRttDir_t       fwd;
    /** Reverse observations */
    qfRttDir_t       rev;
} qfRtt_t;

void qfRttSegment(qfRtt_t           *rtt,
                  uint32_t          seq,
                  uint32_t          ack,
                  uint32_t          tsval,
                  uint32_t          tsecr,
                  uint32_t          ms,
                  uint8_t           tcpflags,
                  unsigned          reverse);

#endif /* idem */
