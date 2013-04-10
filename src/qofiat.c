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

#define _YAF_SOURCE_
#include <yaf/qofiat.h>
#include <yaf/yaftab.h>

static const float kFlightBreakIATDev = 2.0;
static const uint32_t kMinFlightSegments = 3;
static const uint32_t kFlightAlpha = 8;

uint32_t    qfIatSegment(qfIat_t *iatv, uint32_t oct, uint32_t ms) {
    uint32_t iat;
    
    /* short-circuit empty segments */
    if (!oct) return 0;

    /* add segment interarrival time */
    if (iatv->seg_lms) {
        iat = ms - iatv->seg_lms;
        sstMeanAdd(&(iatv->seg), iat);
    } else {
        iat = 0;
    }

    iatv->seg_lms = ms;
    
    /* detect flight break */
    if ((iatv->seg.n > kMinFlightSegments) &&
        (iat > (kFlightBreakIATDev * sstStdev(&iatv->seg))))
    {
        sstLinSmoothAdd(&iatv->flight, iatv->flight_len);
        iatv->flight_len = 0;
    }
    
    /* track statistics for this flight */
    iatv->flight_len += oct;
    
    
    // FIXME debug code tear me out please
//    iatv->sample[iatv->sampleidx] = iat;
//    iatv->sampleidx = (iatv->sampleidx + 1) % 512;
    
    return iat;
}

void qfIatDump(uint64_t fid, qfIat_t *iatv) {
    fprintf(stderr, "flow %5llu iat n %2u min %4u max %4u mean %6.2f stdev %6.2f\n",
            fid, iatv->seg.n, iatv->seg.min, iatv->seg.max, iatv->seg.mean,
            sstStdev(&iatv->seg));
    fprintf(stderr, "           fli      min %4u max %4u smooth %u\n",
            iatv->flight.min, iatv->flight.max, iatv->flight.val);
    // FIXME debug code remove me
//    if (iatv->seg.n > 30) {
//        fprintf(stderr, "iatsample <- c(");
//        for (int i = 0; (i < 512) && (i < iatv->seg.n); i++) {
//            fprintf(stderr, "%6u, ", iatv->sample[i]);
//        }
//        fprintf(stderr, ")\n");
//    }
}