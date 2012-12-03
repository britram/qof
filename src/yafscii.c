/*
 ** yafscii.c
 ** YAF flow printer
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2006-2012 Carnegie Mellon University. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell
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
#include <yaf/autoinc.h>
#include <airframe/mio.h>
#include <airframe/mio_config.h>
#include <airframe/mio_source_file.h>
#include <airframe/mio_sink_file.h>
#include <airframe/logconfig.h>
#include <airframe/daeconfig.h>
#include <airframe/airutil.h>
#include <airframe/privconfig.h>
#include <yaf/yafcore.h>

typedef struct ytContext_st {
    fBuf_t          *fbuf;
    yfFlow_t        flow;
} ytContext_t;

static uint32_t     yaft_flows = 0;

static gboolean     yaft_tabular = FALSE;
static gboolean     yaft_mac = FALSE;
static gboolean     yaft_first = TRUE;
static gboolean     yaft_print_header = FALSE;

static uint32_t     yaft_cliflags = MIO_F_CLI_FILE_IN |
                                    MIO_F_CLI_DIR_IN |
                                    MIO_F_CLI_DEF_STDIN |
                                    MIO_F_CLI_FILE_OUT |
                                    MIO_F_CLI_DIR_OUT |
                                    MIO_F_CLI_DEF_STDOUT;

AirOptionEntry yaft_optentries[] = {
    AF_OPTION( "tabular", (char)0, 0, AF_OPT_TYPE_NONE, &yaft_tabular,
      "Print flows in tabular format", NULL ),
    AF_OPTION( "mac", (char)0, 0, AF_OPT_TYPE_NONE, &yaft_mac,
               "Print mac addresses (when used with --tabular)", NULL ),
    AF_OPTION( "print-header", (char)0, 0, AF_OPT_TYPE_NONE, &yaft_print_header,
               "Print column headers for tabular format", NULL),
    AF_OPTION_END
};

static void ytParseOptions(
    int             *argc,
    char            **argv[]) {

    AirOptionCtx *aoctx = NULL;

    aoctx = air_option_context_new("", argc, argv, yaft_optentries);

    mio_add_option_group(aoctx, yaft_cliflags);
    daec_add_option_group(aoctx);
    privc_add_option_group(aoctx);
    logc_add_option_group(aoctx, "yafscii", VERSION);

    air_option_context_set_help_enabled(aoctx);

    air_option_context_parse(aoctx);
    if (yaft_mac && !yaft_tabular) {
        g_warning("--mac requires --tabular");
    }
    if (yaft_print_header && !yaft_tabular) {
        g_warning("--print-header requires --tabular");
    }

    air_option_context_free(aoctx);
}

static gboolean ytOpenSource(
    MIOSource               *source,
    void                    *vctx,
    uint32_t                *flags,
    GError                  **err)
{
    ytContext_t          *yx = (ytContext_t *)vctx;

    /* start reading a YAF file */
    if (!(yx->fbuf = yfReaderForFP(yx->fbuf, mio_fp(source), err))) {
        *flags |= MIO_F_CTL_SOURCECLOSE;
        if (g_error_matches(*err, FB_ERROR_DOMAIN, FB_ERROR_EOF)) {
            g_clear_error(err);
        } else {
            *flags |= MIO_F_CTL_ERROR;
        }
        return FALSE;
    }

    return TRUE;
}

static gboolean ytProcess(
    MIOSource               *source,
    MIOSink                 *sink,
    void                    *vctx,
    uint32_t                *flags,
    GError                  **err)
{
    ytContext_t          *yx = (ytContext_t *)vctx;
    gboolean                ok;

    /* Print first line column headers if tabular */
    if (yaft_tabular && yaft_first && yaft_print_header) {
        yfPrintColumnHeaders(mio_fp(sink), yaft_mac, err);
        yaft_first = FALSE;
    }

    /* read a flow */
/*    if ((ok = yfReadFlowExtended(yx->fbuf, &(yx->flow), err))) { */
    if ((ok = yfReadFlow(yx->fbuf, &(yx->flow), err))) {
        ++yaft_flows;
        if (yaft_tabular) {
            ok = yfPrintDelimited(mio_fp(sink), &(yx->flow), yaft_mac, err);
        } else {
            ok = yfPrint(mio_fp(sink), &(yx->flow), err);
        }
    }

    /* handle error */
    if (!ok) {
        /* check for EOF */
        if (g_error_matches(*err, FB_ERROR_DOMAIN, FB_ERROR_EOF)) {
            g_clear_error(err);
            *flags |= MIO_F_CTL_SOURCECLOSE;
            ok = TRUE;
        } else {
            /* die on actual error */
            *flags |= MIO_F_CTL_ERROR;
        }
    }

    return ok;
}


int main (
    int                 argc,
    char                *argv[]) {

    GError              *err = NULL;
    ytContext_t         yx;
    MIOSource           source;
    MIOSink             sink;
    MIOAppDriver        adrv;
    uint32_t            miodflags;
    int                 rv = 0;

    /* parse options */
    ytParseOptions(&argc, &argv);

    /* check to make sure the data structures are sane */
    yfAlignmentCheck();

    /* set up logging */
    if (!logc_setup(&err)) {
        air_opterr("%s", err->message);
    }

    /* fork if necessary */
    if (!daec_setup(&err)) {
        air_opterr("%s", err->message);
    }

    /* initialize MIO option flags */
    miodflags = MIO_F_OPT_SINKLINK;

    if (!mio_config_source(&source, yaft_cliflags, &miodflags, &err)) {
        air_opterr("Cannot set up input: %s", err->message);
    }

    /* set up sink */
    if (!mio_config_sink(&source, &sink, "%s.yaf.txt", yaft_cliflags,
                         &miodflags, &err)) {
        air_opterr("Cannot set up output: %s", err->message);
    }

    /* initialize yafscii context */
    yfFlowPrepare(&(yx.flow));
    yx.fbuf = NULL;

    /* set up an app driver */
    adrv.app_open_source = ytOpenSource;
    adrv.app_open_sink = NULL;
    adrv.app_close_source = NULL;
    adrv.app_close_sink = NULL;
    adrv.app_process = ytProcess;

    /* run dispatch loop */
    if (!mio_dispatch_loop(&source, &sink, &adrv, &yx, miodflags, mio_ov_poll,
                           1, mio_ov_poll))
    {
        rv = 1;
    }

    if (yx.fbuf) {
        fBufFree(yx.fbuf);
    }

    g_message("yafscii terminating with %d flows read", yaft_flows);

    return rv;
}
