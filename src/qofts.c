/**
 ** qofts.c
 ** Timestamp rate tracking for QoF
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
#include <yaf/qofts.h>

static const uint64_t k2e32 = 1L << 32;

void qfTimestampSegment(qfTsOpt_t   *ts,
                        uint32_t    val,
                        uint32_t    ecr,
                        uint32_t    lms)
{
    if (!ts->itslms && !ts->itsval) {
        /* initialize */
        ts->itslms = lms;
        ts->itsval = val;
    } else {
        /* track most recent timestamp
           NOTE wrap detection assumes this is only called on
           empty segment or sequence number advance */

        /* increment wrap counters if necessary */
        if (val < ts->ltsval) {
            ts->valwrap++;
        }
        if (lms < ts->ltslms) {
            ts->lmswrap++;
        }
        
        /* save current values */
        ts->ltslms = lms;
        ts->ltsval = val;
    }
    
}

