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

static uint64_t tmims;
static uint64_t tmifid;
static gboolean tmirev;

void qfDynTmiOpen(const char *filename) {
    tmifp = fopen(filename, "w");
    
    if (tmifp) {
        fprintf(tmifp, "%8s %10s %1s %10s %10s %10s %6s %6s %6s %6s %6s\n",
                "time", "flow", "d", "seq", "fan", "rcseq", "iat", "rttm", "rttc", "rtx", "reo");
    } else {
        g_warning("cannot open dynamics TMI file %s: %s", filename, strerror(errno));
    }
}

void qfDynTmiFlow(uint64_t ms, uint64_t fid, gboolean rev) {
    if (tmifp) {
        tmims = ms;
        tmifid = fid;
        tmirev = rev;
    }
}

void qfDynTmiDynamics(uint32_t seq, uint32_t fan, uint32_t rcseq,
                      uint32_t iat, uint32_t rttm, uint32_t rttc,
                      uint32_t rtx, uint32_t reo)
{
    if (tmifp) {
        fprintf(tmifp, "%8.3f %10llu %1s %10u %10u %10u %6u %6u %6u %6u %6u\n",
                tmims / 1000.0, tmifid, tmirev ? "r" : "f",
                seq, fan, rcseq, iat,
                rttm, rttc == UINT32_MAX ? 0 : rttc,
                rtx, reo);
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