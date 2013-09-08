/**
 ** qofrtt.c
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

#define _YAF_SOURCE_
#include <yaf/qofrtt.h>
#include <yaf/yaftab.h>

static int qfWrapCompare(uint32_t a, uint32_t b) {
    return a == b ? 0 : ((a - b) & 0x80000000) ? -1 : 1;
}


static void qfRttSetAckWait(qfRttDir_t  *dir,
                            uint32_t    seq,
                            uint32_t    ms)
{
    dir->ackwait = 1;
    dir->ecrwait = 0;
    dir->lms = ms;
    dir->tsack = seq;
}


static void qfRttSetEcrWait(qfRttDir_t  *dir,
                            uint32_t    tsval,
                            uint32_t    ms)
{
    dir->ackwait = 0;
    dir->ecrwait = 1;
    dir->lms = ms;
    dir->tsack = tsval;
}

static void qfRttSample(qfRtt_t     *rtt)
{
    if (rtt->fwd.obs_ms && rtt->rev.obs_ms) {
        sstMeanAdd(&rtt->val, rtt->fwd.obs_ms + rtt->rev.obs_ms);
    }
}


void qfRttSegment(qfRtt_t           *rtt,
                  uint32_t          seq,
                  uint32_t          ack,
                  uint32_t          tsval,
                  uint32_t          tsecr,
                  uint32_t          ms,
                  uint8_t           tcpflags,
                  unsigned          reverse)
{
    qfRttDir_t    *fdir, *rdir;
    
    /* select which side we're looking at */
    if (reverse) {
        fdir = &rtt->rev;
        rdir = &rtt->fwd;
    } else {
        fdir = &rtt->fwd;
        rdir = &rtt->rev;
    }

    if (fdir->ackwait && (tcpflags & YF_TF_ACK) &&
        qfWrapCompare(ack, fdir->tsack) >= 0)
    {
        /* got an ACK we were waiting for */
        fdir->obs_ms = ms - fdir->lms;
        qfRttSample(rtt);
        fdir->ackwait = 0;
        if (tsval) {
            qfRttSetEcrWait(rdir, tsval, ms);
        }
            
    } else if (fdir->ecrwait && qfWrapCompare(tsecr, fdir->tsack) >= 0) {
        /* got a TSECR we were waiting for */
        fdir->obs_ms = ms - fdir->lms;
        qfRttSample(rtt);
        sstMeanAdd(&rtt->val, fdir->obs_ms + rdir->obs_ms);
        fdir->ecrwait = 0;
        qfRttSetAckWait(rdir, seq, ms);
    } else if (!rdir->ackwait && !rdir->ecrwait) {
        qfRttSetAckWait(rdir, seq, ms);
    }
}