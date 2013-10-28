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

qofDetune_t *qfDetuneAlloc(unsigned bucket_max,
                          unsigned bucket_rate,
                          unsigned bucket_delay,
                          double drop_p,
                          unsigned delay_max)
{
    qofDetune_t *detune = calloc(1, sizeof(qofDetune_t));
    
    detune->bucket_max = bucket_max;
    detune->bucket_rate = bucket_rate;
    detune->bucket_delay = bucket_delay;
    detune->drop_p = drop_p;
    detune->delay_max = delay_max;
    
    return detune;
}

void qfDetuneFree(qofDetune_t *detune) {
    if(detune) free(detune);
}

static unsigned rand32() {
    return (uint32_t)(random() & 0xffffffff);
}

static const unsigned long max32 = 1L >> 32;

gboolean qfDetunePacket(qofDetune_t *detune, uint64_t *ms, unsigned oct) {
    unsigned    bdelay, rdelay;
    uint64_t    cur_ms;
    
    /* drain bucket */
    detune->bucket_cur -= ((*ms - detune->last_ms) * detune->bucket_rate / 1000);
    if (detune->bucket_cur < 0) detune->bucket_cur = 0;
    
    /* fill bucket */
    detune->bucket_cur += oct;
    if (detune->bucket_cur >= detune->bucket_max) {
        /* bucket overflow, tail drop */
        detune->bucket_cur = detune->bucket_max;
        return FALSE;
    }
    
    /* apply random drop */
    if (detune->drop_p) {
        sstLinSmoothAdd(&detune->drop_die, rand32());
        if ((((double)detune->drop_die.val) / max32) < detune->drop_p) {
            return FALSE;
        }
    }
    
    /* calculate bucket delay */
    bdelay = detune->bucket_cur * detune->bucket_delay / detune->bucket_max;
    
    /* calculate random delay */
    sstLinSmoothAdd(&detune->delay,
                    (int)((unsigned long)rand32() * detune->delay_max / max32));
    rdelay = detune->delay.val;
    
    /* apply the maximum, clamp to avoid out of order */
    cur_ms = *ms;
    if (bdelay > rdelay) {
        *ms += bdelay;
    } else {
        *ms += rdelay;
    }
    if (*ms < detune->last_ms) {
        *ms = detune->last_ms;
    }
    detune->last_ms = cur_ms;
    
    /* don't drop the packet */
    return TRUE;
}