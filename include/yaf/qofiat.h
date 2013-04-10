/**
** qofiat.c
** Interarrival time and TCP flight data structures and algorithms for QoF
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

#ifndef _QOF_IAT_H_
#define _QOF_IAT_H_

#include <yaf/autoinc.h>
#include <yaf/streamstat.h>

typedef struct qfIat_st {
    sstMean_t       seg;
    sstLinSmooth_t  flight;
    uint32_t        seg_lms;
    uint32_t        flight_len;
} qfIat_t;

uint32_t    qfIatSegment(qfIat_t *iatv, uint32_t oct, uint32_t ms);

void qfIatDump(uint64_t fid, qfIat_t *iatv);

#endif