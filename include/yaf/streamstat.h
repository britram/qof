/**
 ** streamstat.h
 ** basic streaming statistics data structure for QoF
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2013 Brian Trammell
 **            Algorithm taken from Knuth, The Art of Computer Programming,
 **            volume 2, third edition, page 232
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell <brian@trammell.ch>
 ** ------------------------------------------------------------------------
 ** QoF is made available under the terms of the GNU Public License (GPL)
 ** Version 2, June 1991
 ** ------------------------------------------------------------------------
 */

#ifndef _QOF_STREAMSTAT_H_
#define _QOF_STREAMSTAT_H_

#include <yaf/autoinc.h>
typedef struct sstV_st {
    uint32_t    last;
    uint32_t    n;
    uint32_t    min;
    uint32_t    max;
    double      mean;
    double      s;
} sstV_t;

void sstInit(sstV_t *v);
void sstAdd(sstV_t *v, uint32_t x);
double sstVariance(sstV_t *v);
double sstStdev(sstV_t *v);

#endif /* idem hack */