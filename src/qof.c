/**
 ** qof.c
 ** QoF flow meter; based on Yet Another Flowmeter (YAF) by CERT/NetSA
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2006-2011 Carnegie Mellon University. All Rights Reserved.
 ** Copyright (C) 2013.     Brian Trammell.             All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell
 ** ------------------------------------------------------------------------
 ** QoF is made available under the terms of the
 ** GNU General Public License (GPL) Version 2, June 1991
 ** ------------------------------------------------------------------------
 */

#define _YAF_SOURCE_
#include <qof/autoinc.h>
#include <airframe/logconfig.h>
#include <airframe/privconfig.h>
#include <airframe/airutil.h>
#include <airframe/airopt.h>
#include <qof/yafcore.h>
#include <qof/yaftab.h>
#include <qof/yafrag.h>

#include "qofltrace.h"
#include "yafstat.h"

#include "qofconfig.h"

#include "qofdetune.h"


/* FIXME determine if we want to be more dynamic about this */
#define YAF_SNAPLEN 96

/* Configuration configuration */
static char         *qof_yaml_config = NULL;

/* QoF context */
static qfContext_t  qfctx;

/* I/O configuration */
static int          yaf_opt_rotate_period = 0;
static int          yaf_opt_template_rtx_period = 0;
static int          yaf_opt_stats_period = 0;

/* Detune configuration */
#if QOF_ENABLE_DETUNE
static int          qof_detune_bucket_max = 0;
static int          qof_detune_bucket_rate = 0;
static int          qof_detune_bucket_delay = 0;
static double       qof_detune_drop_p = 0.0;
static int          qof_detune_delay_max = 0;
static int          qof_detune_alpha = 4;
#endif

/* global quit flag */
int                 yaf_quit = 0;

#define THE_LAME_80COL_FORMATTER_STRING "\n\t\t\t\t"

// FIXME refactor all of this into YAML configuration for QoF

/* Local derived configutation */

AirOptionEntry yaf_optent_core[] = {
    AF_OPTION( "in", 'i', 0, AF_OPT_TYPE_STRING, &(qfctx.ictx.inuri),
               THE_LAME_80COL_FORMATTER_STRING"Input (file, - for stdin; "
               "interface)", "inspec"),
    AF_OPTION( "out", 'o', 0, AF_OPT_TYPE_STRING, &(qfctx.octx.outspec),
               THE_LAME_80COL_FORMATTER_STRING"Output (file, - for stdout; "
               "file prefix,"THE_LAME_80COL_FORMATTER_STRING"address)",
               "outspec"),
    AF_OPTION( "filter", 'F', 0, AF_OPT_TYPE_STRING, &(qfctx.ictx.bpf_expr),
               THE_LAME_80COL_FORMATTER_STRING"BPF filtering expression",
               "expression"),
    AF_OPTION( "rotate", 'R', 0, AF_OPT_TYPE_INT, &yaf_opt_rotate_period,
               THE_LAME_80COL_FORMATTER_STRING"Rotate output files every n "
               "seconds ", "sec" ),
    AF_OPTION( "lock", 'k', 0, AF_OPT_TYPE_NONE, &(qfctx.octx.enable_lock),
               THE_LAME_80COL_FORMATTER_STRING"Use exclusive .lock files on "
               "output for"THE_LAME_80COL_FORMATTER_STRING"concurrency", NULL),
    AF_OPTION( "stats", (char)0, 0, AF_OPT_TYPE_INT,
               &(yaf_opt_stats_period),
               THE_LAME_80COL_FORMATTER_STRING"Export yaf process stats "
               "every n seconds "THE_LAME_80COL_FORMATTER_STRING
               "[300 (5 min)]", NULL),
    AF_OPTION("ipfix",(char)0, 0, AF_OPT_TYPE_STRING,
              &(qfctx.octx.transport),
              THE_LAME_80COL_FORMATTER_STRING"Export via IPFIX (tcp, udp, "
              "sctp) to CP at -o","protocol"),
    AF_OPTION( "yaml", (char)0, 0, AF_OPT_TYPE_STRING,
              &qof_yaml_config,
              THE_LAME_80COL_FORMATTER_STRING"Read configuration "
              " from YAML file [./qof.yaml]","file"),
    AF_OPTION_END
};


AirOptionEntry yaf_optent_exp[] = {
    AF_OPTION( "template-refresh", (char)0, 0, AF_OPT_TYPE_INT,
              &yaf_opt_template_rtx_period,
              THE_LAME_80COL_FORMATTER_STRING"UDP template refresh period "
              "[600 sec, 10 m]", "sec"),
    AF_OPTION( "observation-domain", (char)0, 0, AF_OPT_TYPE_INT,
               &qfctx.octx.odid,
               THE_LAME_80COL_FORMATTER_STRING"Set observationDomainID on exported"
               THE_LAME_80COL_FORMATTER_STRING"messages [1]", "odId" ),
    AF_OPTION_END
};

AirOptionEntry yaf_optent_ipfix[] = {
    AF_OPTION( "ipfix-port", (char)0, 0, AF_OPT_TYPE_STRING,
               &(qfctx.octx.connspec.svc), THE_LAME_80COL_FORMATTER_STRING
               "Select IPFIX export port [4739, 4740]", "port" ),
    AF_OPTION( "tls", (char)0, 0, AF_OPT_TYPE_NONE,
               &qfctx.octx.enable_tls,
               THE_LAME_80COL_FORMATTER_STRING"Use TLS/DTLS to secure IPFIX "
               "export", NULL ),
    AF_OPTION( "tls-ca", (char)0, 0, AF_OPT_TYPE_STRING,
               &(qfctx.octx.connspec.ssl_ca_file),
               THE_LAME_80COL_FORMATTER_STRING"Specify TLS Certificate "
               "Authority file", "cafile" ),
    AF_OPTION( "tls-cert", (char)0, 0, AF_OPT_TYPE_STRING,
               &(qfctx.octx.connspec.ssl_cert_file),
               THE_LAME_80COL_FORMATTER_STRING"Specify TLS Certificate file",
               "certfile" ),
    AF_OPTION( "tls-key", (char)0, 0, AF_OPT_TYPE_STRING,
               &(qfctx.octx.connspec.ssl_key_file),
               THE_LAME_80COL_FORMATTER_STRING"Specify TLS Private Key file",
               "keyfile" ),
    AF_OPTION_END
};

#if QOF_ENABLE_DETUNE
AirOptionEntry qof_optent_detune[] = {
    AF_OPTION( "detune-bucket-max", (char)0, 0, AF_OPT_TYPE_INT,
              &qof_detune_bucket_max, THE_LAME_80COL_FORMATTER_STRING
              "Size of detune bucket [0 = disable]", "bytes" ),
    AF_OPTION( "detune-bucket-rate", (char)0, 0, AF_OPT_TYPE_INT,
              &qof_detune_bucket_rate, THE_LAME_80COL_FORMATTER_STRING
              "Drain rate of detune bucket [0 = disable]", "bytes/s" ),
    AF_OPTION( "detune-bucket-delay", (char)0, 0, AF_OPT_TYPE_INT,
              &qof_detune_bucket_delay, THE_LAME_80COL_FORMATTER_STRING
              "Imparted delay of full bucket (linear) [0]", "ms" ),
    AF_OPTION( "detune-random-drop", (char)0, 0, AF_OPT_TYPE_DOUBLE,
              &qof_detune_drop_p, THE_LAME_80COL_FORMATTER_STRING
              "Probability of post-bucket random drop [0.0]", "p" ),
    AF_OPTION( "detune-random-delay", (char)0, 0, AF_OPT_TYPE_INT,
              &qof_detune_delay_max, THE_LAME_80COL_FORMATTER_STRING
              "Maximum post-bucket random delay [0]", "ms" ),
    AF_OPTION( "detune-random-alpha", (char)0, 0, AF_OPT_TYPE_INT,
              &qof_detune_alpha, THE_LAME_80COL_FORMATTER_STRING
              "Random drop/delay linear smoothing weight [4]", "packets" ),
    AF_OPTION_END
};
#endif

/**
 * yfParseOptions
 *
 * parses the command line options via calls to the Airframe
 * library functions
 *
 *
 *
 */
static void yfParseOptions(
    int             *argc,
    char            **argv[]) {

    AirOptionCtx    *aoctx = NULL;
    GError          *err = NULL;


    aoctx = air_option_context_new("", argc, argv, yaf_optent_core);

    air_option_context_add_group(aoctx, "export", "Export Options:",
                                 THE_LAME_80COL_FORMATTER_STRING"Show help "
                                 "for export format options", yaf_optent_exp);
    air_option_context_add_group(aoctx, "ipfix", "IPFIX Options:",
                                 THE_LAME_80COL_FORMATTER_STRING"Show help "
                                 "for IPFIX export options", yaf_optent_ipfix);
#if QOF_ENABLE_DETUNE
    air_option_context_add_group(aoctx, "detune", "Detuning Options:",
                                 THE_LAME_80COL_FORMATTER_STRING"Show help "
                                 "for detuning options", qof_optent_detune);
#endif
    
    privc_add_option_group(aoctx);

    logc_add_option_group(aoctx, "qof", VERSION " (\"" CUTE_VERSION "\")");

    air_option_context_set_help_enabled(aoctx);

    air_option_context_parse(aoctx);

    /* set up logging and privilege drop */
    if (!logc_setup(&err)) {
        air_opterr("%s", err->message);
    }

    if (!privc_setup(&err)) {
        air_opterr("%s", err->message);
    }

    /* copy periods into octx, multiply seconds to milliseconds */
    if (yaf_opt_rotate_period) {
        qfctx.octx.rotate_period = yaf_opt_rotate_period * 1000;
    }
    
    if (yaf_opt_stats_period) {
        qfctx.octx.stats_period = yaf_opt_stats_period * 1000;
    }
    
    if (yaf_opt_template_rtx_period) {
        qfctx.octx.template_rtx_period = yaf_opt_template_rtx_period * 1000;
    }

#if QOF_ENABLE_DETUNE
    /* detune if necessary */
    if (qof_detune_bucket_max || qof_detune_bucket_rate ||
        qof_detune_delay_max || qof_detune_drop_p)
    {
        /* if we have a bucket, we need a rate */
        if (!(qof_detune_bucket_max && qof_detune_bucket_rate)) {
            qof_detune_bucket_max = 0;
            qof_detune_bucket_rate = 0;
        }
        
        /* allocate detune */
        qfctx.ictx.detune = qfDetuneAlloc(qof_detune_bucket_max,
                                          qof_detune_bucket_rate,
                                          qof_detune_bucket_delay,
                                          qof_detune_drop_p,
                                          qof_detune_delay_max,
                                          qof_detune_alpha);
        g_warning("detuning enabled");
    }
#endif
    
    /* done with options parsing */
    air_option_context_free(aoctx);
}



/**
 *
 *
 *
 *
 *
 */
static void yfQuit() {
    yaf_quit++;
}

/**
 *
 *
 *
 *
 *
 */
static void yfQuitInit() {
    struct sigaction sa, osa;

    /* install quit flag handlers */
    sa.sa_handler = yfQuit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT,&sa,&osa)) {
        g_error("sigaction(SIGINT) failed: %s", strerror(errno));
    }

    sa.sa_handler = yfQuit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGTERM,&sa,&osa)) {
        g_error("sigaction(SIGTERM) failed: %s", strerror(errno));
    }
}

/**
 *
 *
 *
 *
 *
 */
int main (
    int             argc,
    char            *argv[])
{
    gboolean        loop_ok = TRUE;

    /* check structure alignment */
    qfInternalTemplateCheck();

    /* zero out context */
    memset(&qfctx, 0, sizeof(qfContext_t));
    
    /* fill in configuration defaults */
    qfConfigDefaults(&qfctx.cfg, &qfctx.ictx, &qfctx.octx);
    
    /* parse options */
    yfParseOptions(&argc, &argv);
    
    /* parse configuration file */
    if (!qfConfigDotfile(&qfctx.cfg, qof_yaml_config, &qfctx.err)) {
        qfContextTerminate(&qfctx);
    }

    /* configure default template if necessary */
    qfConfigDefaultTemplate(&qfctx.cfg);
    
    /* Set up quit handler */
    yfQuitInit();

    /* Set up context */
    qfContextSetup(&qfctx);

    /* Start statistics collection */
    yfStatInit(&qfctx);
    
    /* runloop */
    loop_ok = qfTraceMain(&qfctx);
    
    /* Finish statistics collection */
    yfStatComplete();

    /* Close packet source */
    qfTraceClose(qfctx.ictx.pktsrc);

    /* Clean up! */
    qfContextTeardown(&qfctx);
    
    /* Print exit message */
    if (loop_ok) {
        g_message("qof terminating");
    } else {
        qfContextTerminate(&qfctx);
    }

    return loop_ok ? 0 : 1;
}
