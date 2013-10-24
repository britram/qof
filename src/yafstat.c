/*
 ** yafstat.c
 ** YAF Statistics Signal Handler
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2006-2012 Carnegie Mellon University. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell, Chris Inacio
 ** ------------------------------------------------------------------------
 ** @OPENSOURCE_HEADER_START@
 ** Use of the YAF system and related source code is subject to the terms
 ** of the following licenses:
 **
 ** GNU Public License (GPL) Rights pursuant to Version 2, June 1991
 ** Government Purpose License Rights (GPLR) pursuant to DFARS 252.227.7013
 **
 ** NO WARRANTY
 **
 ** ANY INFORMATION, MATERIALS, SERVICES, INTELLECTUAL PROPERTY OR OTHER
 ** PROPERTY OR RIGHTS GRANTED OR PROVIDED BY CARNEGIE MELLON UNIVERSITY
 ** PURSUANT TO THIS LICENSE (HEREINAFTER THE "DELIVERABLES") ARE ON AN
 ** "AS-IS" BASIS. CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY
 ** KIND, EITHER EXPRESS OR IMPLIED AS TO ANY MATTER INCLUDING, BUT NOT
 ** LIMITED TO, WARRANTY OF FITNESS FOR A PARTICULAR PURPOSE,
 ** MERCHANTABILITY, INFORMATIONAL CONTENT, NONINFRINGEMENT, OR ERROR-FREE
 ** OPERATION. CARNEGIE MELLON UNIVERSITY SHALL NOT BE LIABLE FOR INDIRECT,
 ** SPECIAL OR CONSEQUENTIAL DAMAGES, SUCH AS LOSS OF PROFITS OR INABILITY
 ** TO USE SAID INTELLECTUAL PROPERTY, UNDER THIS LICENSE, REGARDLESS OF
 ** WHETHER SUCH PARTY WAS AWARE OF THE POSSIBILITY OF SUCH DAMAGES.
 ** LICENSEE AGREES THAT IT WILL NOT MAKE ANY WARRANTY ON BEHALF OF
 ** CARNEGIE MELLON UNIVERSITY, EXPRESS OR IMPLIED, TO ANY PERSON
 ** CONCERNING THE APPLICATION OF OR THE RESULTS TO BE OBTAINED WITH THE
 ** DELIVERABLES UNDER THIS LICENSE.
 **
 ** Licensee hereby agrees to defend, indemnify, and hold harmless Carnegie
 ** Mellon University, its trustees, officers, employees, and agents from
 ** all claims or demands made against them (and any related losses,
 ** expenses, or attorney's fees) arising out of, or relating to Licensee's
 ** and/or its sub licensees' negligent use or willful misuse of or
 ** negligent conduct or willful misconduct regarding the Software,
 ** facilities, or other rights or assistance granted by Carnegie Mellon
 ** University under this License, including, but not limited to, any
 ** claims of product liability, personal injury, death, damage to
 ** property, or violation of any laws or regulations.
 **
 ** Carnegie Mellon University Software Engineering Institute authored
 ** documents are sponsored by the U.S. Department of Defense under
 ** Contract FA8721-05-C-0003. Carnegie Mellon University retains
 ** copyrights in all material produced under this contract. The U.S.
 ** Government retains a non-exclusive, royalty-free license to publish or
 ** reproduce these documents, or allow others to do so, for U.S.
 ** Government purposes only pursuant to the copyright license under the
 ** contract clause at 252.227.7013.
 **
 ** @OPENSOURCE_HEADER_END@
 ** ------------------------------------------------------------------------
 */

#define _YAF_SOURCE_
#include "yafstat.h"
#include <qof/yaftab.h>
#include <qof/yafrag.h>
#include <qof/decode.h>

static uint32_t yaf_do_stat = 0;
static GTimer *yaf_fft = NULL;
static qfContext_t *statctx = NULL;
static uint64_t yaf_dropped = 0;

static void yfSigUsr1()
{
    ++yaf_do_stat;
}

void yfStatInit(
    qfContext_t         *ctx)
{
    struct sigaction sa, osa;

    /* install usr1 handler */
    sa.sa_handler = yfSigUsr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1,&sa,&osa)) {
        g_error("sigaction(SIGUSR1) failed: %s", strerror(errno));
    }

    /* stash statistics context */
    statctx = ctx;

    /* start the timer */
    yaf_fft = g_timer_new();
    g_timer_start(yaf_fft);
}

static void yfStatDump()
{
    uint64_t numPackets;
    numPackets = yfFlowDumpStats(statctx->flowtab, yaf_fft);
    yfFragDumpStats(statctx->fragtab, numPackets);
    yfDecodeDumpStats(statctx->dectx, numPackets);

    if (yaf_dropped) {
        g_warning("Capture dropped %llu packets.", yaf_dropped);
    }
}

void yfStatDumpLoop()
{
    if (yaf_do_stat) {
        --yaf_do_stat;
        yfStatDump();
    }
}

void yfStatComplete()
{
    g_timer_stop(yaf_fft);
    yfStatDump();
}

double yfStatElapsedTime()
{
    return g_timer_elapsed(yaf_fft, NULL);
}

void yfStatReportDropped(uint64_t dropped) {
    yaf_dropped = dropped;
}

uint64_t yfStatGetDropped(void) {
    return yaf_dropped;
}
