/**
 * @internal
 *
 ** qofltrace.h
 ** QoF libtrace input support
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C)      2013 Brian Trammell.             All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell <brian@trammell.ch>
 ** ------------------------------------------------------------------------
 ** QoF is made available under the terms of the
 ** GNU General Public License (GPL) Version 2, June 1991
 ** ------------------------------------------------------------------------
 */

#ifndef _QOF_LTRACE_H_
#define _QOF_LTRACE_H_

#include <qof/autoinc.h>
#include <libtrace.h>
#include "qofconfig.h"

struct qfTraceSource_st;
typedef struct qfTraceSource_st qfTraceSource_t;

qfTraceSource_t *qfTraceOpen(const char *uri,
                             const char *bpf,
                             int snaplen,
                             GError **err);

void qfTraceClose(qfTraceSource_t *lts);


gboolean qfTraceMain(qfContext_t *ctx);

#endif