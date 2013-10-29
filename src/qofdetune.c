/**
 ** qofdetune.c
 ** Packet stream detuning (experimental) for QoF.
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
#include "qofdetune.h"

#if QOF_ENABLE_DETUNE

qofDetune_t *qfDetuneAlloc(unsigned bucket_max,
                          unsigned bucket_rate,
                          unsigned bucket_delay,
                          double drop_p,
                          unsigned delay_max,
                          unsigned alpha)
{
    srandom(time(NULL));
    
    qofDetune_t *detune = calloc(1, sizeof(qofDetune_t));
    
    detune->bucket_max = bucket_max;
    detune->bucket_rate = bucket_rate;
    detune->bucket_delay = bucket_delay;
    detune->drop_p = drop_p;
    detune->delay_max = delay_max;
    
    sstLinSmoothInit(&detune->delay);
    detune->delay.alpha = alpha;
    sstLinSmoothInit(&detune->drop_die);
    detune->drop_die.alpha = alpha;
    
    return detune;
}

void qfDetuneFree(qofDetune_t *detune) {
    if(detune) free(detune);
}

static unsigned randbits() {
    return (uint32_t)(random() & 0x3fffffffU);
}

static const unsigned long randmax = 0x40000000UL;

gboolean qfDetunePacket(qofDetune_t *detune, uint64_t *ms, unsigned oct) {
    unsigned    bdelay = 0, rdelay = 0;
    uint64_t    cur_ms;
    
    if (detune->bucket_max) {
        /* drain bucket */
        if (detune->last_ms && detune->bucket_cur && (*ms > detune->last_ms)) {
            detune->bucket_cur -= ((*ms - detune->last_ms) *
                                   detune->bucket_rate / 1000);
            if (detune->bucket_cur < 0) {
                detune->bucket_cur = 0;
            }
        }
        
        /* fill bucket */
        detune->bucket_cur += oct;
        if (detune->bucket_cur > detune->stat_highwater) {
            detune->stat_highwater = detune->bucket_cur;
        }
        if (detune->bucket_cur >= detune->bucket_max) {
            /* bucket overflow, tail drop */
            detune->bucket_cur = detune->bucket_max;
            detune->stat_bucket_drop++;
            return FALSE;
        }
    }
    
    /* apply random drop */
    if (detune->drop_p) {
        sstLinSmoothAdd(&detune->drop_die, randbits());
        if ((((double)((unsigned)detune->drop_die.val)) / randmax) < detune->drop_p) {
            detune->stat_random_drop++;
            return FALSE;
        }
    }
    
    /* calculate bucket delay */
    if (detune->bucket_max && detune->bucket_delay) {
        bdelay = detune->bucket_cur * detune->bucket_delay / detune->bucket_max;
    }
    
    /* calculate random delay */
    if (detune->delay_max) {
        sstLinSmoothAdd(&detune->delay,
                        (int)((unsigned long)randbits() * detune->delay_max / randmax));
        rdelay = detune->delay.val;
    }
    
    /* apply the maximum, clamp to avoid out of order */
    cur_ms = *ms;
    if (bdelay || rdelay) {
        if (bdelay > rdelay) {
            *ms += bdelay;
        } else {
            *ms += rdelay;
        }
        if (*ms < detune->last_ms) {
            *ms = detune->last_ms;
        }
        sstMeanAdd(&detune->stat_delay, (int)(*ms - cur_ms));
    }
    
    /* advance last millisecond counter */
    detune->last_ms = *ms;
    
    /* if we made it here, don't drop the packet */
    return TRUE;
}

uint64_t qfDetuneDumpStats(qofDetune_t          *detune,
                           uint64_t             flowPacketCount)
{
    uint64_t droppedPacketCount = detune->stat_bucket_drop +
                                  detune->stat_random_drop;
    uint64_t totalPacketCount = flowPacketCount + droppedPacketCount;
    
    g_debug("Detune got %llu packets, dropped %llu (%3.2f%%)",
            totalPacketCount, droppedPacketCount,
            ((double)(droppedPacketCount)/(double)(totalPacketCount) * 100) );
    if (detune->stat_bucket_drop) {
        g_debug("  %llu (%3.2f%%) tail drops", detune->stat_bucket_drop,
                ((double)(detune->stat_bucket_drop)/(double)(droppedPacketCount) * 100) );
    }
    if (detune->stat_random_drop) {
        g_debug("  %llu (%3.2f%%) random losses", detune->stat_random_drop,
                ((double)(detune->stat_random_drop)/(double)(droppedPacketCount) * 100) );
    }
    if (!detune->stat_bucket_drop && detune->stat_highwater) {
        g_debug("  high water mark %u (%3.2f%%)", detune->stat_highwater,
                ((double)(detune->stat_highwater)/(double)(detune->bucket_max) * 100) );
        
    }
    if (detune->stat_delay.mean) {
        g_debug("  mean imparted delay %u ms (min %d max %d stdev %.2f)",
                (unsigned int)detune->stat_delay.mean,
                detune->stat_delay.mm.min,
                detune->stat_delay.mm.max,
                sstStdev(&detune->stat_delay));
    }
    
    return totalPacketCount;
}


#endif