/**
 ** qofopt.h
 ** Option tracking data structures and function prototypes for QoF
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

#ifndef _QOF_OPT_H_
#define _QOF_OPT_H_

#include <qof/autoinc.h>
#include <qof/yafcore.h>
#include <qof/decode.h>

#define QF_OPT_ECT0             0x00000001 /* observed an ECT(0) codepoint */
#define QF_OPT_ECT1             0x00000002 /* observed an ECT(1) codepoint */
#define QF_OPT_CE               0x00000004 /* observed a CE codepoint */
#define QF_OPT_TS               0x00000010 /* observed a TS option */
#define QF_OPT_SACK             0x00000020 /* observed a SACK option */
#define QF_OPT_WS               0x00000040 /* observed a window scale option */

/* qfOpt_t is defined in yafcore.h to break a circular include loop 
   involving decode.h. At some point we should fix this less inelegantly. */

void qfOptSegment(qfOpt_t *qo,
                  yfTCPInfo_t *tcpinfo,
                  yfIPInfo_t *ipinfo,
                  uint16_t oct);

#endif