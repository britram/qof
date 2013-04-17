/**
 ** qofdyntmi.c
 ** TCP dynamics detailed output
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
#include "qofdyntmi.h"

static FILE *tmifp = NULL;

void qfDynTmiOpen(const char *filename) {
    tmifp = fopen(filename, "w");
    
    if (tmifp) {
        fprintf(tmifp, "%10s %1s %10s %10s %6s %6s %6s\n",
                "flow", "d", "seq", "fan", "iat", "rttm", "rttc");
    } else {
        g_warning("cannot open dynamics TMI file %s: %s", filename, strerror(errno));
    }
}

void qfDynTmiWrite(uint64_t fid, gboolean rev, uint32_t seq,
                   uint32_t fan, uint32_t iat,
                   uint32_t rttm, uint32_t rttc)
{
    if (tmifp) {
        fprintf(tmifp, "%10llu %1s %10u %10u %6u %6u %6u\n",
                fid, rev ? "r" : "f", seq, fan, iat, rttm, rttc);
    }
}
                         
void qfDynTmiClose()
{
    if (tmifp) {
        fflush(tmifp);
        fclose(tmifp);
        tmifp = NULL;
    }
}