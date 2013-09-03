/**
 ** qofrtt.h
 ** RTT estimation data structures and algorithms for QoF
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

typedef struct qfRtt_st {
    /* Mean/min/max RTT (per biflow) */
    sstMean_t       val;
    /* Next ack/tsecr expected in the forward direction */
    uint32_t        fwd_tsack;
    /* Time seq/tsval seen ( + rttx = ctime) in the forward direction */
    uint32_t        fwd_lms;
    /* observed forward RTT (fwd synack / rev valecr) */
    uint32_t        fwd_ms;
    /* Next ack/tsecr expected in the reverse direction */
    uint32_t        rev_tsack;
    /* Time seq/tsval seen ( + rttx = ctime) in the reverse direction */
    uint32_t        rev_lms;
    /* observed reverse RTT (rev synack / fwd valecr) */
    uint32_t        rev_ms;
    /* flags */
    uint32_t        flags;
} qfRtt_t;

