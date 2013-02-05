/**
 ** yaf.c
 ** Yet Another Flow generator
 * **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2006-2011 Carnegie Mellon University. All Rights Reserved.
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
#include <airframe/logconfig.h>
#include <airframe/privconfig.h>
#include <airframe/airutil.h>
#include <airframe/airopt.h>
#include <yaf/yafcore.h>
#include <yaf/yaftab.h>
#include <yaf/yafrag.h>
#include <yaf/qofrtt.h>

#include "yafcap.h"
#include "yafstat.h"
#include "qofctx.h"
#if YAF_ENABLE_DAG
#include "yafdag.h"
#endif
#if YAF_ENABLE_NAPATECH
#include "yafpcapx.h"
#endif

/* FIXME determine if we want to be more dynamic about this */
#define YAF_SNAPLEN 128

/* Configuration configuration */
static char         *qof_cfgfile = NULL;

/* I/O configuration */
static yfConfig_t    yaf_config = YF_CONFIG_INIT;
static int           yaf_opt_rotate = 0;
static uint64_t      yaf_rotate_ms = 0;
static gboolean      yaf_opt_caplist_mode = FALSE;
static char          *yaf_opt_ipfix_transport = NULL;
static gboolean      yaf_opt_ipfix_tls = FALSE;

/* GOption managed flow table options */
static int          yaf_opt_idle = 300;
static int          yaf_opt_active = 1800;
static int          yaf_opt_rttring = 0;
static int          yaf_opt_max_flows = 0;
static gboolean     yaf_opt_force_read_all = FALSE;
static gboolean     yaf_opt_uniflow_mode = FALSE;
static gboolean     yaf_opt_silk_mode = FALSE;
static gboolean     yaf_opt_extra_stats_mode = FALSE;

/* GOption managed fragment table options */
static int          yaf_opt_max_frags = 0;
static gboolean     yaf_opt_nofrag = FALSE;

/* GOption managed decoder options and derived decoder config */
static gboolean     yaf_opt_ip4_mode = FALSE;
static gboolean     yaf_opt_ip6_mode = FALSE;
static uint16_t     yaf_reqtype;
static gboolean     yaf_opt_gre_mode = FALSE;
static gboolean     yaf_opt_mac_mode = FALSE;

/* GOption managed core export options */
static gboolean     yaf_opt_ip6map_mode = FALSE;

/* global quit flag */
int                 yaf_quit = 0;

/* Runtime functions */

typedef void *(*yfLiveOpen_fn)(const char *, int, int *, GError **);
static yfLiveOpen_fn yaf_liveopen_fn = NULL;

typedef gboolean (*yfLoop_fn)(yfContext_t *);
static yfLoop_fn yaf_loop_fn = NULL;

typedef void (*yfClose_fn)(void *);
static yfClose_fn yaf_close_fn = NULL;

#define THE_LAME_80COL_FORMATTER_STRING "\n\t\t\t\t"

// FIXME refactor all of this into YAML configuration for QoF

/* Local derived configutation */

AirOptionEntry yaf_optent_core[] = {
    AF_OPTION( "in", 'i', 0, AF_OPT_TYPE_STRING, &yaf_config.inspec,
               THE_LAME_80COL_FORMATTER_STRING"Input (file, - for stdin; "
               "interface)", "inspec"),
    AF_OPTION( "out", 'o', 0, AF_OPT_TYPE_STRING, &yaf_config.outspec,
               THE_LAME_80COL_FORMATTER_STRING"Output (file, - for stdout; "
               "file prefix,"THE_LAME_80COL_FORMATTER_STRING"address)",
               "outspec"),
    AF_OPTION( "live", 'P', 0, AF_OPT_TYPE_STRING, &yaf_config.livetype,
               THE_LAME_80COL_FORMATTER_STRING"Capture from interface in -i; "
               "type is "THE_LAME_80COL_FORMATTER_STRING"[pcap] or dag",
               "type"),
    AF_OPTION( "filter", 'F', 0, AF_OPT_TYPE_STRING, &yaf_config.bpf_expr,
               THE_LAME_80COL_FORMATTER_STRING"BPF filtering expression",
               "expression"),
    AF_OPTION( "caplist", (char)0, 0, AF_OPT_TYPE_NONE, &yaf_opt_caplist_mode,
               THE_LAME_80COL_FORMATTER_STRING"Read ordered list of input "
               "files from "THE_LAME_80COL_FORMATTER_STRING"file in -i", NULL),
    AF_OPTION( "rotate", 'R', 0, AF_OPT_TYPE_INT, &yaf_opt_rotate,
               THE_LAME_80COL_FORMATTER_STRING"Rotate output files every n "
               "seconds ", "sec" ),
    AF_OPTION( "lock", 'k', 0, AF_OPT_TYPE_NONE, &yaf_config.lockmode,
               THE_LAME_80COL_FORMATTER_STRING"Use exclusive .lock files on "
               "output for"THE_LAME_80COL_FORMATTER_STRING"concurrency", NULL),
    AF_OPTION( "no-stats", (char)0, 0, AF_OPT_TYPE_NONE, &yaf_config.nostats,
               THE_LAME_80COL_FORMATTER_STRING"Turn off stats option records "
               "IPFIX export", NULL),
    AF_OPTION( "stats", (char)0, 0, AF_OPT_TYPE_INT, &yaf_config.stats,
               THE_LAME_80COL_FORMATTER_STRING"Export yaf process stats "
               "every n seconds "THE_LAME_80COL_FORMATTER_STRING
               "[300 (5 min)]", NULL),
    AF_OPTION("ipfix",(char)0, 0, AF_OPT_TYPE_STRING, &yaf_opt_ipfix_transport,
              THE_LAME_80COL_FORMATTER_STRING"Export via IPFIX (tcp, udp, "
              "sctp) to CP at -o","protocol"),
    AF_OPTION( "yaml", (char)0, 0, AF_OPT_TYPE_STRING, &qof_cfgfile,
              THE_LAME_80COL_FORMATTER_STRING"Read additional configuration "
              " from YAML file"THE_LAME_80COL_FORMATTER_STRING"(currently used"
              "for interface mapping)","file"),
    AF_OPTION_END
};

AirOptionEntry yaf_optent_dec[] = {
    AF_OPTION( "no-frag", (char)0, 0, AF_OPT_TYPE_NONE, &yaf_opt_nofrag,
               THE_LAME_80COL_FORMATTER_STRING"Disable IP fragment reassembly",
               NULL ),
    AF_OPTION( "max-frags", (char)0, 0, AF_OPT_TYPE_INT, &yaf_opt_max_frags,
               THE_LAME_80COL_FORMATTER_STRING"Maximum size of fragment table "
               "[0]", "fragments" ),
    AF_OPTION( "ip4-only", (char)0, 0, AF_OPT_TYPE_NONE, &yaf_opt_ip4_mode,
               THE_LAME_80COL_FORMATTER_STRING"Only process IPv4 packets",
               NULL ),
    AF_OPTION( "ip6-only", (char)0, 0, AF_OPT_TYPE_NONE, &yaf_opt_ip6_mode,
               THE_LAME_80COL_FORMATTER_STRING"Only process IPv6 packets",
               NULL ),
    AF_OPTION( "gre-decode", (char)0, 0, AF_OPT_TYPE_NONE, &yaf_opt_gre_mode,
               THE_LAME_80COL_FORMATTER_STRING"Decode GRE encapsulated "
               "packets", NULL ),
    AF_OPTION( "mac", (char)0, 0, AF_OPT_TYPE_NONE, &yaf_opt_mac_mode,
               THE_LAME_80COL_FORMATTER_STRING"Export MAC-layer information",
               NULL ),
    AF_OPTION( "noerror", (char)0, 0, AF_OPT_TYPE_NONE, &yaf_config.noerror,
               THE_LAME_80COL_FORMATTER_STRING"Do not error out on single "
               "PCAP file issue"THE_LAME_80COL_FORMATTER_STRING" with "
               "multiple inputs", NULL ),
    AF_OPTION_END
};

AirOptionEntry yaf_optent_flow[] = {
    AF_OPTION( "idle-timeout", 'I', 0, AF_OPT_TYPE_INT, &yaf_opt_idle,
               THE_LAME_80COL_FORMATTER_STRING"Idle flow timeout [300, 5m]",
               "sec" ),
    AF_OPTION( "active-timeout", 'A', 0, AF_OPT_TYPE_INT, &yaf_opt_active,
               THE_LAME_80COL_FORMATTER_STRING"Active flow timeout [1800, "
               "30m]", "sec" ),
    // FIXME make this nonconfigurable once we know how well it works
    AF_OPTION ( "rtt", 'r', 0, AF_OPT_TYPE_INT, &yaf_opt_rttring,
                THE_LAME_80COL_FORMATTER_STRING"track RTT with n segments [0]",
               "seg-count" ),
    AF_OPTION( "max-flows", (char)0, 0, AF_OPT_TYPE_INT, &yaf_opt_max_flows,
               THE_LAME_80COL_FORMATTER_STRING"Maximum size of flow table [0]",
               "flows" ),
    AF_OPTION( "udp-temp-timeout", (char)0, 0, AF_OPT_TYPE_INT,
               &yaf_config.yaf_udp_template_timeout,
               THE_LAME_80COL_FORMATTER_STRING"UDP template timeout period "
               "[600 sec, 10 m]", "sec"),
    AF_OPTION("force-read-all", (char)0, 0, AF_OPT_TYPE_NONE,
              &yaf_opt_force_read_all, THE_LAME_80COL_FORMATTER_STRING"Force "
              "read of any out of sequence packets", NULL),
    AF_OPTION_END
};

AirOptionEntry yaf_optent_exp[] = {
    AF_OPTION( "silk", (char)0, 0, AF_OPT_TYPE_NONE, &yaf_opt_silk_mode,
               THE_LAME_80COL_FORMATTER_STRING"Clamp octets to 32 bits, "
               "note continued in"THE_LAME_80COL_FORMATTER_STRING
               "flowEndReason.", NULL ),
    AF_OPTION( "uniflow", (char)0, 0, AF_OPT_TYPE_NONE, &yaf_opt_uniflow_mode,
               THE_LAME_80COL_FORMATTER_STRING"Write uniflows for "
               "compatibility", NULL ),
    AF_OPTION( "force-ip6-export", (char)0, 0, AF_OPT_TYPE_NONE,
               &yaf_opt_ip6map_mode, THE_LAME_80COL_FORMATTER_STRING"Export "
               "all IPv4 addresses as IPv6 in "THE_LAME_80COL_FORMATTER_STRING
               "::FFFF/96 [N/A]", NULL ),
    AF_OPTION( "observation-domain", (char)0, 0, AF_OPT_TYPE_INT,
               &yaf_config.odid, THE_LAME_80COL_FORMATTER_STRING
               "Set observationDomainID on exported"
               THE_LAME_80COL_FORMATTER_STRING"messages [1]", "odId" ),
    AF_OPTION( "flow-stats", (char)0, 0, AF_OPT_TYPE_NONE,
               &yaf_opt_extra_stats_mode, THE_LAME_80COL_FORMATTER_STRING
               "Export extra flow attributes and statistics ", NULL),
    AF_OPTION( "delta", (char)0, 0, AF_OPT_TYPE_NONE, &yaf_config.deltamode,
               THE_LAME_80COL_FORMATTER_STRING"Export packet and octet counts "
               "using delta "THE_LAME_80COL_FORMATTER_STRING
               "information elements", NULL),
    AF_OPTION ("ingress", (char)0, 0, AF_OPT_TYPE_INT, &yaf_config.ingressInt,
               THE_LAME_80COL_FORMATTER_STRING"Set ingressInterface field in "
               "flow template", NULL),
    AF_OPTION( "egress", (char)0, 0, AF_OPT_TYPE_INT, &yaf_config.egressInt,
               THE_LAME_80COL_FORMATTER_STRING"Set egressInterface field in "
               "flow template", NULL),
#if YAF_ENABLE_DAG_SEPARATE_INTERFACES
    AF_OPTION( "dag-interface", (char)0, 0, AF_OPT_TYPE_NONE,
               &yaf_config.dagInterface, THE_LAME_80COL_FORMATTER_STRING
               "Export DAG interface numbers in export records",
               NULL ),
#endif
#if YAF_ENABLE_NAPATECH_SEPARATE_INTERFACES
    AF_OPTION( "napatech-interface", (char)0, 0, AF_OPT_TYPE_NONE,
               &yaf_config.pcapxInterface, THE_LAME_80COL_FORMATTER_STRING
               "Export Napatech interface numbers in export records",
               NULL ),
#endif
    AF_OPTION_END
};

AirOptionEntry yaf_optent_ipfix[] = {
    AF_OPTION( "ipfix-port", (char)0, 0, AF_OPT_TYPE_STRING,
               &(yaf_config.connspec.svc), THE_LAME_80COL_FORMATTER_STRING
               "Select IPFIX export port [4739, 4740]", "port" ),
    AF_OPTION( "tls", (char)0, 0, AF_OPT_TYPE_NONE, &yaf_opt_ipfix_tls,
               THE_LAME_80COL_FORMATTER_STRING"Use TLS/DTLS to secure IPFIX "
               "export", NULL ),
    AF_OPTION( "tls-ca", (char)0, 0, AF_OPT_TYPE_STRING,
               &(yaf_config.connspec.ssl_ca_file),
               THE_LAME_80COL_FORMATTER_STRING"Specify TLS Certificate "
               "Authority file", "cafile" ),
    AF_OPTION( "tls-cert", (char)0, 0, AF_OPT_TYPE_STRING,
               &(yaf_config.connspec.ssl_cert_file),
               THE_LAME_80COL_FORMATTER_STRING"Specify TLS Certificate file",
               "certfile" ),
    AF_OPTION( "tls-key", (char)0, 0, AF_OPT_TYPE_STRING,
               &(yaf_config.connspec.ssl_key_file),
               THE_LAME_80COL_FORMATTER_STRING"Specify TLS Private Key file",
               "keyfile" ),
    AF_OPTION_END
};


/**
 * yfCleanVersionString
 *
 * takes in a string defined by autconf, fields seperated
 * by "|" characters and turns it into a better formatted
 * (80 columns or less, etc.) string to pass into the
 * options processing machinery
 *
 * @param verNumStr string containing the version number
 * @param capbilStr string containing the build capabilities
 *                  string
 *
 * @return an allocated formatted string for the version switch
 *         that describes the yaf build
 *
 */
static char *yfCleanVersionString (
    const char *verNumStr,
    const char *capbilStr) {

    unsigned int formatStringSize = 0;
    const unsigned int MaxLineSize = 80;
    const unsigned int TabSize = 8;
    char *resultString;

    formatStringSize = strlen(verNumStr) + 1;
    if (MaxLineSize > strlen(capbilStr)) {
        formatStringSize += strlen(capbilStr);
        resultString = g_malloc(formatStringSize + 1);
        strcpy(resultString, verNumStr);
        strcat(resultString, "\n");
        strcat(resultString, capbilStr);
        strcat(resultString, "\n");
        return resultString;
    } else {
        unsigned int newLineCounters = 0;
        const char *indexPtr = capbilStr + MaxLineSize;
        const char *prevIndexPtr = capbilStr;
        do {
            while ('|' != *indexPtr && indexPtr > prevIndexPtr) {
                indexPtr--;
            }
            if (indexPtr == prevIndexPtr) {
                /* no division point within MaxLineSize characters */
                indexPtr = strchr(indexPtr, '|');
                if (NULL == indexPtr) {
                    indexPtr = prevIndexPtr + strlen(prevIndexPtr) - 1;
                }
            }
            prevIndexPtr = indexPtr+1;
            indexPtr += (MaxLineSize - TabSize) + 1;
            newLineCounters++;
        } while ((MaxLineSize - TabSize) < strlen(prevIndexPtr));

        formatStringSize += strlen(capbilStr) + (2 * newLineCounters) + 1;
        resultString = g_malloc(formatStringSize);
        strcpy(resultString, verNumStr);
        strcat(resultString, "\n");
        prevIndexPtr = capbilStr;
        indexPtr = capbilStr + MaxLineSize;
        do {
            while ('|' != *indexPtr && indexPtr > prevIndexPtr) {
                indexPtr--;
            }
            if (indexPtr == prevIndexPtr) {
                /* no division point within MaxLineSize characters */
                indexPtr = strchr(indexPtr, '|');
                if (NULL == indexPtr) {
                    indexPtr = prevIndexPtr + strlen(prevIndexPtr) - 1;
                }
            }
            strncat(resultString, prevIndexPtr, indexPtr - prevIndexPtr);
            strcat(resultString, "\n\t");
            prevIndexPtr = indexPtr+1;
            indexPtr += (MaxLineSize - TabSize) + 1;
        } while ((MaxLineSize - TabSize) < strlen(prevIndexPtr));
        strcat(resultString, prevIndexPtr);

        return resultString;
    }

    return NULL;
}

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
    char            *versionString;


    aoctx = air_option_context_new("", argc, argv, yaf_optent_core);

    air_option_context_add_group(aoctx, "decode", "Decoder Options:",
                                 THE_LAME_80COL_FORMATTER_STRING"Show help "
                                 "for packet decoder options", yaf_optent_dec);
    air_option_context_add_group(aoctx, "flow", "Flow table Options:",
                                 THE_LAME_80COL_FORMATTER_STRING"Show help "
                                 "for flow table options", yaf_optent_flow);
    air_option_context_add_group(aoctx, "export", "Export Options:",
                                 THE_LAME_80COL_FORMATTER_STRING"Show help "
                                 "for export format options", yaf_optent_exp);
    air_option_context_add_group(aoctx, "ipfix", "IPFIX Options:",
                                 THE_LAME_80COL_FORMATTER_STRING"Show help "
                                 "for IPFIX export options", yaf_optent_ipfix);

    privc_add_option_group(aoctx);

    versionString = yfCleanVersionString(VERSION, YAF_ACONF_STRING_STR);

    logc_add_option_group(aoctx, "yaf", versionString);

    air_option_context_set_help_enabled(aoctx);

    air_option_context_parse(aoctx);

    /* set up logging and privilege drop */
    if (!logc_setup(&err)) {
        air_opterr("%s", err->message);
    }

    if (!privc_setup(&err)) {
        air_opterr("%s", err->message);
    }
    
    /* process ip4mode and ip6mode */
    if (yaf_opt_ip4_mode && yaf_opt_ip6_mode) {
        g_warning("cannot run in both ip4-only and ip6-only modes; "
                  "ignoring these flags");
        yaf_opt_ip4_mode = FALSE;
        yaf_opt_ip6_mode = FALSE;
    }

    if (yaf_opt_ip4_mode) {
        yaf_reqtype = YF_TYPE_IPv4;
    } else if (yaf_opt_ip6_mode) {
        yaf_reqtype = YF_TYPE_IPv6;
    } else {
        yaf_reqtype = YF_TYPE_IPANY;
    }

    /* process core library options */
    if (yaf_opt_ip6map_mode) {
        yfWriterExportMappedV6(TRUE);
    }
    // FIXME reverse the sense of this -- delta mode should be default
    // FIXME check to see whether this should be part of SiLK mode...
    if (!yaf_config.deltamode) {
        yfWriterExportTotals(TRUE);
    }
    
    /* process RTT tracking option */
    qfRttRingSize(yaf_opt_rttring);
    
    // FIXME harmonize with YAML config -- anon mode is selected there
    
    /* Pre-process input options */
    if (yaf_config.livetype) {
        /* can't use caplist with live */
        if (yaf_opt_caplist_mode) {
            air_opterr("Please choose only one of --live or --caplist");
        }

        /* select live capture type */
        if ((*yaf_config.livetype == (char)0) ||
            (strncmp(yaf_config.livetype, "pcap", 4) == 0))
        {
            /* live capture via pcap (--live=pcap or --live) */
            yaf_liveopen_fn = (yfLiveOpen_fn)yfCapOpenLive;
            yaf_loop_fn = (yfLoop_fn)yfCapMain;
            yaf_close_fn = (yfClose_fn)yfCapClose;
#if YAF_ENABLE_DAG
        } else if (strncmp(yaf_config.livetype, "dag", 3) == 0) {
            /* live capture via dag (--live=dag) */
            yaf_liveopen_fn = (yfLiveOpen_fn)yfDagOpenLive;
            yaf_loop_fn = (yfLoop_fn)yfDagMain;
            yaf_close_fn = (yfClose_fn)yfDagClose;
#endif
#if YAF_ENABLE_NAPATECH
        } else if (strncmp(yaf_config.livetype, "napatech", 8) == 0) {
            /* live capture via napatech adapter (--live=napatech) */
            yaf_liveopen_fn = (yfLiveOpen_fn)yfPcapxOpenLive;
            yaf_loop_fn = (yfLoop_fn)yfPcapxMain;
            yaf_close_fn = (yfClose_fn)yfPcapxClose;
#endif
        } else {
            /* unsupported live capture type */
            air_opterr("Unsupported live capture type %s", yaf_config.livetype);
        }

        /* Require an interface name for live input */
        if (!yaf_config.inspec) {
            air_opterr("--live requires interface name in --in");
        }

    } else {
        /* Use pcap loop and close functions */
        yaf_loop_fn = (yfLoop_fn)yfCapMain;
        yaf_close_fn =(yfClose_fn)yfCapClose;

        /* Default to stdin for no input */
        if (!yaf_config.inspec || !strlen(yaf_config.inspec)) {
            yaf_config.inspec = "-";
        }
    }

    /* calculate live rotation delay in milliseconds */
    yaf_rotate_ms = yaf_opt_rotate * 1000;
    yaf_config.rotate_ms = yaf_rotate_ms;

    if (yaf_config.stats == 0) {
        yaf_config.nostats = TRUE;
    }

    /* Pre-process output options */
    if (yaf_opt_ipfix_transport) {
        /* set default port */
        if (!yaf_config.connspec.svc) {
            yaf_config.connspec.svc = yaf_opt_ipfix_tls ? "4740" : "4739";
        }

        /* Require a hostname for IPFIX output */
        if (!yaf_config.outspec) {
            air_opterr("--ipfix requires hostname in --out");
        }

        /* set hostname */
        yaf_config.connspec.host = yaf_config.outspec;

        if ((*yaf_opt_ipfix_transport == (char)0) ||
            (strcmp(yaf_opt_ipfix_transport, "sctp") == 0))
        {
            if (yaf_opt_ipfix_tls) {
                yaf_config.connspec.transport = FB_DTLS_SCTP;
            } else {
                yaf_config.connspec.transport = FB_SCTP;
            }
        } else if (strcmp(yaf_opt_ipfix_transport, "tcp") == 0) {
            if (yaf_opt_ipfix_tls) {
                yaf_config.connspec.transport = FB_TLS_TCP;
            } else {
                yaf_config.connspec.transport = FB_TCP;
            }
        } else if (strcmp(yaf_opt_ipfix_transport, "udp") == 0) {
            if (yaf_opt_ipfix_tls) {
                yaf_config.connspec.transport = FB_DTLS_UDP;
            } else {
                yaf_config.connspec.transport = FB_UDP;
            }
            if (yaf_config.yaf_udp_template_timeout == 0) {
                yaf_config.yaf_udp_template_timeout = 600000;
            } else {
                /* convert to milliseconds */
                yaf_config.yaf_udp_template_timeout =
                    yaf_config.yaf_udp_template_timeout * 1000;
            }
    } else {
        air_opterr("Unsupported IPFIX transport protocol %s",
                   yaf_opt_ipfix_transport);
        }

        /* grab TLS password from environment */
        if (yaf_opt_ipfix_tls) {
            yaf_config.connspec.ssl_key_pass = getenv("YAF_TLS_PASS");
        }

        /* mark that a network connection is requested for this spec */
        yaf_config.ipfixNetTrans = TRUE;

    } else {
        if (!yaf_config.outspec || !strlen(yaf_config.outspec)) {
            if (yaf_rotate_ms) {
                /* Require a path prefix for IPFIX output */
                air_opterr("--rotate requires prefix in --out");
            } else {
                /* Default to stdout for no output without rotation */
                yaf_config.outspec = "-";
            }
        }
    }

    /* Check for stdin/stdout is terminal */
    if ((strlen(yaf_config.inspec) == 1) && yaf_config.inspec[0] == '-') {
        /* Don't open stdin if it's a terminal */
        if (isatty(fileno(stdin))) {
            air_opterr("Refusing to read from terminal on stdin");
        }
    }

    if ((strlen(yaf_config.outspec) == 1) && yaf_config.outspec[0] == '-') {
        /* Don't open stdout if it's a terminal */
        if (isatty(fileno(stdout))) {
            air_opterr("Refusing to write to terminal on stdout");
        }
    }
    
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
    GError          *err = NULL;
    yfContext_t     ctx = YF_CTX_INIT;
    int             datalink;
    gboolean        loop_ok = TRUE;

    /* check structure alignment */
    yfAlignmentCheck();

    /* parse options */
    yfParseOptions(&argc, &argv);
    ctx.cfg = &yaf_config;
    
    /* read YAML configuration file */
    /* FIXME, options parsing needs to be refactored in order to properly
       integrate YAML configuration; as long as this is only for interface
       maps, we're good, though. */
    if (!qfParseYamlConfig(&ctx, qof_cfgfile)) {
        g_warning("terminating on YAML parse error");
        exit(1);
    }
    
    /* Set up quit handler */
    yfQuitInit();

    /* open interface if we're doing live capture */
    if (yaf_liveopen_fn) {
        /* open interface */
        if (!(ctx.pktsrc = yaf_liveopen_fn(yaf_config.inspec,
                                           YAF_SNAPLEN,
                                           &datalink, &err)))
        {
            g_warning("Cannot open interface %s: %s", yaf_config.inspec,
                      err->message);
            exit(1);
        }

        /* drop privilege */
        if (!privc_become(&err)) {
            if (g_error_matches(err, PRIVC_ERROR_DOMAIN, PRIVC_ERROR_NODROP)) {
                g_warning("running as root in --live mode, "
                          "but not dropping privilege");
                g_clear_error(&err);
            } else {
                yaf_close_fn(ctx.pktsrc);
                g_warning("Cannot drop privilege: %s", err->message);
                exit(1);
            }
        }
    } else {
        if (yaf_opt_caplist_mode) {
            /* open input file list */
            if (!(ctx.pktsrc = yfCapOpenFileList(yaf_config.inspec, &datalink,
                                                 &err)))
            {
                g_warning("Cannot open packet file list file %s: %s",
                         yaf_config.inspec, err->message);
                exit(1);
            }
        } else {
            /* open input file */
            if (!(ctx.pktsrc = yfCapOpenFile(yaf_config.inspec, &datalink,
                                             &err)))
            {
                g_warning("Cannot open packet file %s: %s",
                          yaf_config.inspec, err->message);
                exit(1);
            }
        }
    }

    if (yaf_opt_mac_mode) {
        yaf_config.macmode = TRUE;
    }

    if (yaf_opt_extra_stats_mode) {
        yaf_config.statsmode = TRUE;
    }

    if (yaf_opt_silk_mode) {
        yaf_config.silkmode = TRUE;
    }

    /* Allocate a packet ring. */
    ctx.pbufring = rgaAlloc(sizeof(yfPBuf_t), 128);

    /* Set up decode context */
    ctx.dectx = yfDecodeCtxAlloc(datalink, yaf_reqtype, yaf_opt_gre_mode);

    /* Set up flow table */
    ctx.flowtab = yfFlowTabAlloc(yaf_opt_idle * 1000,
                                 yaf_opt_active * 1000,
                                 yaf_opt_max_flows,
                                 yaf_opt_uniflow_mode,
                                 yaf_opt_silk_mode,
                                 yaf_opt_mac_mode,
                                 yaf_opt_force_read_all,
                                 yaf_opt_extra_stats_mode);

    /* Set up fragment table - ONLY IF USER SAYS */
    if (!yaf_opt_nofrag) {
        ctx.fragtab = yfFragTabAlloc(30000,
                                     yaf_opt_max_frags);
    }


    /* We have a packet source, an output stream,
       and all the tables we need. Run with it. */

    yfStatInit(&ctx);
    loop_ok = yaf_loop_fn(&ctx);
    yfStatComplete();
    yfCapDumpStats();
#if YAF_ENABLE_DAG
    if (yaf_liveopen_fn) {
        yfDagDumpStats();
    }
#endif
#if YAF_ENABLE_NAPATECH
    if (yaf_liveopen_fn) {
        yfPcapxDumpStats();
    }
#endif

    /* Close packet source */
    yaf_close_fn(ctx.pktsrc);

    /* Clean up! */
    if (ctx.flowtab) {
        yfFlowTabFree(ctx.flowtab);
    }
    if (ctx.fragtab) {
        yfFragTabFree(ctx.fragtab);
    }
    if (ctx.dectx) {
        yfDecodeCtxFree(ctx.dectx);
    }
    if (ctx.pbufring) {
        rgaFree(ctx.pbufring);
    }

    /* Print exit message */
    if (loop_ok) {
        g_debug("yaf terminating");
    } else {
        g_warning("yaf terminating on error: %s", ctx.err->message);
    }

    return loop_ok ? 0 : 1;
}
