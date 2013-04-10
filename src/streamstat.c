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

static const uint32_t kDefaultAlpha = 8;

void sstMeanInit(sstMean_t *v) {
    memset(v, 0, sizeof(*v));
}

void sstMeanAdd(sstMean_t *v, uint32_t x) {
    double pmean;
    
    v->last = x;
    v->n++;
    
    if (v->n == 1) {
        v->max = v->min = x;
        v->mean = x;
        v->s = 0.0;
    } else {
        if (x > v->max) {
            v->max = x;
        } else if (x < v->min) {
            v->min = x;
        }
        
        pmean = v->mean;
        v->mean = v->mean + ((x - v->mean) / v->n);
        v->s = v->s + ((x - pmean) * (x - v->mean));
    }
}

double sstVariance(sstMean_t *v) {
    if (v->n <= 1) return 0.0;
    return v->s / (v->n - 1);
}

double sstStdev(sstMean_t *v) {
    return sqrt(sstVariance(v));
}

void sstLinSmoothInit(sstLinSmooth_t *v) {
    memset(v, 0, sizeof(*v));
}

void sstLinSmoothAdd(sstLinSmooth_t *v, uint32_t x) {
    if (!v->val && !v->min && !v->max) {
        v->val = v->max = v->min = x;
        if (!v->alpha) v->alpha = kDefaultAlpha;
    } else {
        if (x > v->max) {
            v->max = x;
        } else if (x < v->min) {
            v->min = x;
        }
        v->val = (v->val * (v->alpha - 1) + x) / v->alpha;
    }
}
