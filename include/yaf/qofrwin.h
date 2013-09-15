/**
 ** qofrwin.h
 ** Receiver window data structures and function prototypes for QoF
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

#ifndef _QOF_RWIN_H_
#define _QOF_RWIN_H_

#include <yaf/autoinc.h>
#include <yaf/streamstat.h>

/* Receiver window statistics */
typedef struct qfRwin_st {
    /** Receiver window mean/min/max value */
    sstMean_t       val;
    /** Stall counter */
    uint32_t        stall;
    /** Window scale option */
    uint8_t         scale;
} qfRwin_t;

void qfRwinScale(qfRwin_t       *qr,
                 uint8_t        wscale);

void qfRwinSegment(qfRwin_t     *qr,
                   uint32_t     unscaled);

#endif /* idem */