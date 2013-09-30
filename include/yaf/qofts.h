/**
 ** qofts.h
 ** Timestamp rate data structures and function prototypes for QoF
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

#ifndef _QOF_TS_H_
#define _QOF_TS_H_

#include <yaf/autoinc.h>
#include <yaf/streamstat.h>

/** Receiver window statistics structure */
typedef struct qfTsOpt_st {
    /** Timestamp frequency min/mean/max (in Hz) */
    sstMean_t        hz;
    /** Last timestamp value */
    uint32_t         tsval;
    /** Last timestamp time (milliseconds) */
    uint32_t         tslms;
} qfTsOpt_t;

void qfTimestampSegment(qfTsOpt_t   *ts,
                        uint32_t    val,
                        uint32_t    ecr,
                        uint32_t    lms);


#endif /* idem */