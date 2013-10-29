/**
 ** qofdetune.c
 ** Packet stream detuning (experimental) for QoF.
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

#ifndef _QOF_DETUNE_H_
#define _QOF_DETUNE_H_

#include <qof/autoinc.h>

#if QOF_ENABLE_DETUNE

#include <qof/streamstat.h>

typedef struct qofDetune_st {
    /* current leaky bucket occupancy (bytes) */
    int              bucket_cur;
    /* maximum leaky bucket occupancy (bytes) */
    unsigned         bucket_max;
    /* leaky bucket rate (bytes/s) */
    unsigned         bucket_rate;
    /* full bucket delay (ms) */
    unsigned         bucket_delay;
    /* smoothed random drop die */
    sstLinSmooth_t   drop_die;
    /* random drop probability (0..1) */
    double           drop_p;
    /* smoothed additional random delay */
    sstLinSmooth_t   delay;
    /* maximum value for random delay (ms) */
    unsigned         delay_max;
    /* last timestamp (for minimizing random delay) */
    uint64_t         last_ms;
} qofDetune_t;
     
qofDetune_t *qfDetuneInit(unsigned bucket_max,
                          unsigned bucket_rate,
                          unsigned bucket_delay,
                          double drop_p,
                          unsigned delay_max,
                          unsigned alpha);

void qfDetuneFree(qofDetune_t *detune);

gboolean qfDetunePacket(qofDetune_t *detune, uint64_t *ms, unsigned oct);

#endif
#endif