/**
 ** streamstat.c
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

#define _YAF_SOURCE_
#include <yaf/streamstat.h>
#include <math.h>

void sstInit(sstV_t *v) {
    memset(v, 0, sizeof(*v));
}

void sstAdd(sstV_t *v, uint32_t x) {
    double pmean;
    
    v->last = x;
    v->n++;
    
    if (v->n == 1) {
        v->max = v->min = x;
        v->mean = x;
        v->s = 0.0;
    } else {
        if (x > v->max) v->max = x;
        else if (x < v->min) v->min = x;
        
        pmean = v->mean;
        v->mean = v->mean + ((x - v->mean) / v->n);
        v->s = v->s + ((x - pmean) * (x - v->mean));
    }
}

double sstVariance(sstV_t *v) {
    if (v->n <= 1) return 0.0;
    return v->s / (v->n - 1);
}

double sstStdev(sstV_t *v) {
    return sqrt(sstVariance(v));
}
