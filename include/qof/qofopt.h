/**
 ** qofopt.h
 ** Option tracking data structures and function prototypes for QoF
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

#ifndef _QOF_OPT_H_
#define _QOF_OPT_H_

#include <qof/autoinc.h>
#include <qof/yafcore.h>
#include <qof/decode.h>

/* union of all packets in flow */
#define QF_OPT_ECT0             0x00000001 /* observed an ECT(0) codepoint */
#define QF_OPT_ECT1             0x00000002 /* observed an ECT(1) codepoint */
#define QF_OPT_CE               0x00000004 /* observed a CE codepoint */
#define QF_OPT_TS               0x00000010 /* observed a TS option */
#define QF_OPT_SACK             0x00000020 /* observed a SACK option */
#define QF_OPT_WS               0x00000040 /* observed a window scale option */

/* same as above but for last SYN packet */
#define QF_OPT_LSYN_ECT0        0x00000100 /* observed an ECT(0) codepoint */
#define QF_OPT_LSYN_ECT1        0x00000200 /* observed an ECT(1) codepoint */
#define QF_OPT_LSYN_CE          0x00000400 /* observed a CE codepoint */
#define QF_OPT_LSYN_TS          0x00001000 /* observed a TS option */
#define QF_OPT_LSYN_SACK        0x00002000 /* observed a SACK option */
#define QF_OPT_LSYN_WS          0x00004000 /* observed a window scale option */
#define QF_OPT_LSYN_MASK        0x00007700

/* qfOpt_t is defined in yafcore.h to break a circular include loop 
   involving decode.h. At some point we should fix this less inelegantly. */

void qfOptSegment(qfOpt_t *qo,
                  yfTCPInfo_t *tcpinfo,
                  yfIPInfo_t *ipinfo,
                  uint16_t oct);

#endif