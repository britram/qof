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
    /** First timestamp value */
    uint32_t         itsval;
    /** First timestamp time (milliseconds) */
    uint32_t         itslms;
    /** Most recent timestamp value */
    uint32_t         ltsval;
    /** Most recent timestamp time (milliseconds) */
    uint32_t         ltslms;
    /** Timestamp wrap counter */
    uint32_t         valwrap;
    /** Time wrap counter */
    uint32_t         lmswrap;
} qfTsOpt_t;

void qfTimestampSegment(qfTsOpt_t   *ts,
                        uint32_t    val,
                        uint32_t    ecr,
                        uint32_t    lms);



#endif /* idem */