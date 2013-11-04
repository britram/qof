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
    if (ipinfo->ecn & 0x01) qo->flags |= QF_OPT_ECT0;
    if (ipinfo->ecn & 0x02) qo->flags |= QF_OPT_ECT1;
    if ((ipinfo->ecn & 0x03) == 0x03) qo->flags |= QF_OPT_CE;
    if (tcpinfo->tsval) qo->flags |= QF_OPT_TS;
    if (tcpinfo->ws) qo->flags |= QF_OPT_WS;
    if (tcpinfo->sack) qo->flags |= QF_OPT_SACK;
    if (tcpinfo->mss) qo->mss_opt = tcpinfo->mss;
    if (oct > qo->mss) qo->mss = oct;
}
