/**
 ** qofack.c
 ** Acknowledgment tracking functions for QoF
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2013      Brian Trammell. All Rights Reserved
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell <brian@trammell.ch>
 ** ------------------------------------------------------------------------
 ** QoF is made available under the terms of the
 ** GNU General Public License (GPL) Version 2, June 1991
 ** ------------------------------------------------------------------------
 */

#define _YAF_SOURCE_

#include <qof/qofack.h>
#include <qof/decode.h>

static int qfWrapCompare(uint32_t a, uint32_t b) {
    return a == b ? 0 : ((a - b) & 0x80000000) ? -1 : 1;
}

void qfAckSegment(qfAck_t *qa,
                  uint32_t ack,
                  uint32_t sack,
                  uint32_t oct,
                  uint32_t ms)
{
    if (!qa->fan || qfWrapCompare(ack, qa->fan) > 0) {
        qa->fan = ack;
        qa->fanlms = ms;
    } else if (!oct) {
        qa->dup_ct++;
    }
    
    if (sack && sack != QOF_SACK_OK && qfWrapCompare(sack, ack) > 0) {
        qa->sel_ct++;
    }
}