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

typedef struct sstMinMax_st {
    int         min;
    int         max;
} sstMinMax_t;

typedef struct sstMean_st {
    int         last;
    int         n;  
    sstMinMax_t mm;
    double      mean;
    double      s;
} sstMean_t;

typedef struct sstLinSmooth_st {
    int         alpha;
    int         n;
    sstMinMax_t mm;
    int         val;
} sstLinSmooth_t;

void sstMinMaxInit(sstMinMax_t *v);
void sstMinMaxAdd(sstMinMax_t *v, int x);

void sstMeanInit(sstMean_t *v);
void sstMeanAdd(sstMean_t *v, int x);
double sstVariance(sstMean_t *v);
double sstStdev(sstMean_t *v);

void sstLinSmoothInit(sstLinSmooth_t *v);
void sstLinSmoothAdd(sstLinSmooth_t *v, int x);

#endif /* idem hack */