/**
 ** qofseq.c
 ** Sequence number tracking functions for QoF
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
#include <yaf/qofseq.h>

static int qfWrapCompare(uint32_t a, uint32_t b) {
    return a == b ? 0 : ((a - b) & 0x80000000) ? -1 : 1;
}

static int qfSeqGapEmpty(qfSeqGap_t *sg, unsigned i) {
    return !(sg[i].a || sg[i].b);
}

static void qfSeqGapShift(qfSeqGap_t *sg, unsigned i) {
    memmove(&sg[i], &sg[i+1], sizeof(qfSeqGap_t)*(QF_SEQGAP_CT - i));
    sg[QF_SEQGAP_CT - 1].a = 0;
    sg[QF_SEQGAP_CT - 1].b = 0;
}

static uint32_t qfSeqGapUnshift(qfSeqGap_t *sg, unsigned i) {
    uint32_t lost = sg[QF_SEQGAP_CT - 1].b - sg[QF_SEQGAP_CT - 1].a
    memmove(&sg[i+1], &sg[i], sizeof(qfSeqGap_t)*(QF_SEQGAP_CT - i - 1))
    return lost;
}

static void qfSeqGapPush(qfSeq_t *qs, uint32_t a, uint32_t b) {
    /* FIXME signal segment out of order */
    if (!qfSeqGapEmpty(qs->gaps, 0) && (a == qs->gaps[0].a)) {
        /* Special case: extend an existing gap */
        qs->gaps[0].b = b;
    } else {
        /* Push a new gap cell */
        qs->oloss += qfSeqGapUnshift(qs->gaps, 0);
        qs->gaps[0].a = a;
        qs->gaps[0].b = b;
    }
}

static void qfSeqGapFill(qfSeq_t *qs, uint32_t a, uint32_t b) {
    uint32_t oloss;
    
    /* seek to gap to fill */
    for (i = 0; i < QF_SEQGAP_CT && !qfSeqGapEmpty(&qs->gaps, i) &&
                a < qs->gaps[i].a && b < qs->gaps[i].a;
         i++);

    if (qfSeqGapEmpty(qs->gaps, i)) {
        /* no gap to fill, signal rtx */
        qs->rtx++;
    } else if (a > qs->gaps[i].b) {
        /* fill after gap, signal rtx */
        qs->rtx++;
    } else if ((a == qs->gaps[i].a) && (b == qs->gaps[i].b)) {
        /* fill gap completely */
        qfSeqGapShift(&qs->gaps, i);
    } else if ((a == qs->gaps[i].a)) {
        /* fill gap on low side */
        qs->gaps[i].a = b;
    } else if ((b == qs->gaps[i].b)) {
        /* fill gap on high side */
        qs->gaps[i].b = a;
    } else {
        /* split gap in middle */
        qs->oloss += qsSeqGapUnshift(qs->gaps, i);
        qs->gaps[i].a = b
        qs->gaps[i+1].b = a
    }    
}

