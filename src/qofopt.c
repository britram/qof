/**
 ** qofopt.c
 ** Options tracking functions for QoF
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
#include <qof/qofopt.h>
#include <qof/decode.h>

void qfOptSegment(qfOpt_t *qo,
                  yfTCPInfo_t *tcpinfo,
                  yfIPInfo_t *ipinfo,
                  uint16_t oct)
{
    /* ECN characteristics */
    if ((ipinfo->ecn & 0x03) == 0x01) qo->flags |= QF_OPT_ECT1;
    if ((ipinfo->ecn & 0x03) == 0x02) qo->flags |= QF_OPT_ECT0;
    if ((ipinfo->ecn & 0x03) == 0x03) qo->flags |= QF_OPT_CE;
    
    /* TCP options */
    if (tcpinfo->tsval) qo->flags |= QF_OPT_TS;
    if (tcpinfo->ws) qo->flags |= QF_OPT_WS;
    if (tcpinfo->sack == QOF_SACK_OK) qo->flags |= QF_OPT_SACK;

    /* Reported and observed MSS */
    if (tcpinfo->mss) qo->mss_opt = tcpinfo->mss;
    if (oct > qo->mss) qo->mss = oct;
    
    /* Last SYN options and SYN count */
    if (tcpinfo->flags & YF_TF_SYN) {
        qo->flags &= (~QF_OPT_LSYN_MASK);
        ++qo->syncount;
        if ((ipinfo->ecn & 0x03) == 0x01) qo->flags |= QF_OPT_LSYN_ECT1;
        if ((ipinfo->ecn & 0x03) == 0x02) qo->flags |= QF_OPT_LSYN_ECT0;
        if ((ipinfo->ecn & 0x03) == 0x03) qo->flags |= QF_OPT_LSYN_CE;
        if (tcpinfo->tsval) qo->flags |= QF_OPT_LSYN_TS;
        if (tcpinfo->ws) qo->flags |= QF_OPT_LSYN_WS;
        if (tcpinfo->sack == QOF_SACK_OK) qo->flags |= QF_OPT_LSYN_SACK;
    }
}
