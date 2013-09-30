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

void qfTimestampSegment(qfTsOpt_t   *ts,
                        uint32_t    val,
                        uint32_t    ecr,
                        uint32_t    lms)
{
    if (!ts->tslms && !ts->tsval) {
        /* initialize */
        ts->tslms = lms;
        ts->tsval = val;
    } else {
        /* take frequency sample (silly expensive) */
        sstMeanAdd(&ts->hz,
                   (uint32_t)((double)(val - ts->tsval) /
                             ((double)(lms - ts->tslms) / 1000.0)));
    
        /* save current values */
        ts->tslms = lms;
        ts->tsval = val;
    }
}
