/**
 ** qofack.h
 ** Acknowledgment tracking structures and prototypes for QoF
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


#ifndef _QOF_ACK_H_
#define _QOF_ACK_H_

#include <yaf/autoinc.h>

typedef struct qfAck_st {
    /** Final acknowledgment number */
    uint32_t        fan;
    /** Time of lask acknowledgment advance */
    uint32_t        fanlms;
    /** Duplicate acknowledgement count */
    uint32_t        dup_ct;
    /** Selective acklnowledgment count */
    uint32_t        sel_ct;
} qfAck_t;

void qfAckSegment(qfAck_t *qa, uint32_t ack, uint32_t sack,
                  uint32_t oct, uint32_t ms);

#endif