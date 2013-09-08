/**
 ** qofrtt.h
 ** RTT estimation data structures and function prototypes for QoF
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

#ifndef _QOF_RTT_H_
#define _QOF_RTT_H_

#include <yaf/autoinc.h>
#include <yaf/streamstat.h>

/** per-biflow RTT tracking structure */
typedef struct qfRtt_st {
    /** Mean/min/max RTT (per biflow) */
    sstMean_t       val;
    struct {
        /** Next ack/tsecr expected in the forward direction */
        uint32_t    tsack;
        /** Time seq/tsval seen ( + rttx = ctime) in the forward direction */
        uint32_t    lms;
        /** observed forward RTT (fwd synack / rev valecr) */
        uint32_t    obs_ms;
    } fwd;
    struct {
        /** Next ack/tsecr expected in the reverse direction */
        uint32_t    tsack;
        /** Time seq/tsval seen ( + rttx = ctime) in the reverse direction */
        uint32_t    lms;
        /** observed reverse RTT (rev synack / fwd valecr) */
        uint32_t    obs_ms;
    } rev;
    /** control flags */
    uint32_t        flags;
} qfRtt_t;

void qfRttSegment(qfRtt_t           *rtt,
                  uint32_t          seq,
                  uint32_t          ack,
                  uint32_t          tsval,
                  uint32_t          tsecr,
                  uint8_t           tcpflags,
                  unsigned          reverse);

#endif /* idem */
