/*
 ** logconfig.c
 ** Generic glib-based logging configuration support
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2005-2011 Carnegie Mellon University. All Rights Reserved.
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

#define _AIRFRAME_SOURCE_
#include <airframe/logconfig.h>
#include <airframe/daeconfig.h>
#include <airframe/airutil.h>

static char *RCSID __attribute__ ((unused)) =
    "$Id: logconfig.c 18316 2012-03-27 15:38:40Z ecoff_svn $";

#define THE_LAME_80COL_FORMATTER_STRING "\n\t\t\t\t"


typedef struct logc_st {
    /* Options */
    const char     *appname;
    const char     *version;
    char           *logspec;
    char           *loglevel;
    gboolean        opt_version;
    gboolean        opt_verbose;
    /* Logging sockets */
    GIOChannel      *logfile;
} logc_t;

static logc_t logc = { "", "", NULL, NULL, FALSE, FALSE, NULL };

AirOptionEntry logc_optentries[] = {
    AF_OPTION( "log", 'l', 0, AF_OPT_TYPE_STRING, &(logc.logspec),
               THE_LAME_80COL_FORMATTER_STRING"Log facility, log file path, "
               "or 'stderr'", "logspec" ),
    AF_OPTION( "loglevel", 'L', 0, AF_OPT_TYPE_STRING, &(logc.loglevel),
               THE_LAME_80COL_FORMATTER_STRING"Log level (debug, message, "
               "warning, critical,"
               THE_LAME_80COL_FORMATTER_STRING"error, quiet)", "level" ),
    AF_OPTION( "verbose", 'v' , 0, AF_OPT_TYPE_NONE, &(logc.opt_verbose),
               THE_LAME_80COL_FORMATTER_STRING"Verbose logging, equivalent "
               "to -L debug", NULL ),
    AF_OPTION( "version", 'V', 0, AF_OPT_TYPE_NONE, &(logc.opt_version),
               THE_LAME_80COL_FORMATTER_STRING"Print application version and "
               "exit", NULL ),
    AF_OPTION_END
};


gboolean logc_add_option_group(
    AirOptionCtx   *aoctx,
    const char     *appname,
    const char     *version)
{
    g_assert(aoctx != NULL);

    /* store application name and version */
    logc.appname = appname;
    logc.version = version;

    air_option_context_add_group(aoctx, "log", "Logging Options:",
                                 THE_LAME_80COL_FORMATTER_STRING"Show help "
                                 "for logging options", logc_optentries);

    return TRUE;
}


static void logc_print_version() {
    fprintf(stderr,"%s version %s (c) 2000-2012 Carnegie Mellon University.\n",
            logc.appname,logc.version);
    fprintf(stderr,"GNU General Public License (GPL) Rights "
                   "pursuant to Version 2, June 1991\n");
    fprintf(stderr,"Some included library code covered by LGPL 2.1; "
                   "see source for details.\n");
    fprintf(stderr,"Government Purpose License Rights (GPLR) "
                   "pursuant to DFARS 252.227-7013\n");
    fprintf(stderr, "Send bug reports, feature requests, and comments to "
            "netsa-help@cert.org.\n");
}

static gint logc_syslog_level(
    GLogLevelFlags  level) {

    if (level & G_LOG_LEVEL_DEBUG) return LOG_DEBUG;
    if (level & G_LOG_LEVEL_INFO) return LOG_INFO;
    if (level & G_LOG_LEVEL_MESSAGE) return LOG_NOTICE;
    if (level & G_LOG_LEVEL_WARNING) return LOG_WARNING;
    if (level & G_LOG_LEVEL_CRITICAL) return LOG_ERR;
    if (level & G_LOG_LEVEL_ERROR) return LOG_ERR;

    return LOG_DEBUG;
}

static gboolean logc_parse_log_level(
    const char      *levstr,
    GLogLevelFlags  *levflag) {

    if (strcmp("debug",levstr) == 0) {
        *levflag = G_LOG_FLAG_RECURSION |
                   G_LOG_FLAG_FATAL |
                   G_LOG_LEVEL_ERROR |
                   G_LOG_LEVEL_CRITICAL |
                   G_LOG_LEVEL_WARNING |
                   G_LOG_LEVEL_MESSAGE |
                   G_LOG_LEVEL_INFO |
                   G_LOG_LEVEL_DEBUG;
        return TRUE;
    }

    if (strcmp("info",levstr) == 0) {
        *levflag = G_LOG_FLAG_RECURSION |
                   G_LOG_FLAG_FATAL |
                   G_LOG_LEVEL_ERROR |
                   G_LOG_LEVEL_CRITICAL |
                   G_LOG_LEVEL_WARNING |
                   G_LOG_LEVEL_MESSAGE |
                   G_LOG_LEVEL_INFO;
        return TRUE;
    }

    if (strcmp("message",levstr) == 0) {
        *levflag = G_LOG_FLAG_RECURSION |
                   G_LOG_FLAG_FATAL |
                   G_LOG_LEVEL_ERROR |
                   G_LOG_LEVEL_CRITICAL |
                   G_LOG_LEVEL_WARNING |
                   G_LOG_LEVEL_MESSAGE;
        return TRUE;
    }

    if (strcmp("warning",levstr) == 0) {
        *levflag = G_LOG_FLAG_RECURSION |
                   G_LOG_FLAG_FATAL |
                   G_LOG_LEVEL_ERROR |
                   G_LOG_LEVEL_CRITICAL |
                   G_LOG_LEVEL_WARNING;
        return TRUE;
    }

    if (strcmp("critical",levstr) == 0) {
        *levflag = G_LOG_FLAG_RECURSION |
                   G_LOG_FLAG_FATAL |
                   G_LOG_LEVEL_ERROR |
                   G_LOG_LEVEL_CRITICAL;
        return TRUE;
    }

    if (strcmp("error",levstr) == 0) {
        *levflag = G_LOG_FLAG_RECURSION |
                   G_LOG_FLAG_FATAL |
                   G_LOG_LEVEL_ERROR;
        return TRUE;
    }

    if (strcmp("quiet",levstr) == 0) {
        *levflag = 0;
        return TRUE;
    }

    return FALSE;
}

static gboolean logc_parse_syslog_facility(
    const char      *facstr,
    gint            *facility) {

#ifdef LOG_AUTH
    if (strcmp("auth",facstr) == 0) {
        *facility = LOG_AUTH;
        return TRUE;
    }
#endif

#ifdef LOG_AUTHPRIV
    if (strcmp("authpriv",facstr) == 0) {
        *facility = LOG_AUTHPRIV;
        return TRUE;
    }
#endif

#ifdef LOG_CONSOLE
    if (strcmp("console",facstr) == 0) {
        *facility = LOG_CONSOLE;
        return TRUE;
    }
#endif

#ifdef LOG_CRON
    if (strcmp("cron",facstr) == 0) {
        *facility = LOG_CRON;
        return TRUE;
    }
#endif

#ifdef LOG_DAEMON
    if (strcmp("daemon",facstr) == 0) {
        *facility = LOG_DAEMON;
        return TRUE;
    }
#endif

#ifdef LOG_FTP
    if (strcmp("ftp",facstr) == 0) {
        *facility = LOG_FTP;
        return TRUE;
    }
#endif

#ifdef LOG_LPR
    if (strcmp("lpr",facstr) == 0) {
        *facility = LOG_LPR;
        return TRUE;
    }
#endif

#ifdef LOG_MAIL
    if (strcmp("mail",facstr) == 0) {
        *facility = LOG_MAIL;
        return TRUE;
    }
#endif

#ifdef LOG_NEWS
    if (strcmp("news",facstr) == 0) {
        *facility = LOG_NEWS;
        return TRUE;
    }
#endif

#ifdef LOG_SECURITY
    if (strcmp("security",facstr) == 0) {
        *facility = LOG_SECURITY;
        return TRUE;
    }
#endif

#ifdef LOG_USER
    if (strcmp("user",facstr) == 0) {
        *facility = LOG_USER;
        return TRUE;
    }
#endif

#ifdef LOG_UUCP
    if (strcmp("uucp",facstr) == 0) {
        *facility = LOG_UUCP;
        return TRUE;
    }
#endif

#ifdef LOG_LOCAL0
    if (strcmp("local0",facstr) == 0) {
        *facility = LOG_LOCAL0;
        return TRUE;
    }
#endif

#ifdef LOG_LOCAL1
    if (strcmp("local1",facstr) == 0) {
        *facility = LOG_LOCAL1;
        return TRUE;
    }
#endif

#ifdef LOG_LOCAL2
    if (strcmp("local2",facstr) == 0) {
        *facility = LOG_LOCAL2;
        return TRUE;
    }
#endif

#ifdef LOG_LOCAL3
    if (strcmp("local3",facstr) == 0) {
        *facility = LOG_LOCAL3;
        return TRUE;
    }
#endif

#ifdef LOG_LOCAL4
    if (strcmp("local4",facstr) == 0) {
        *facility = LOG_LOCAL4;
        return TRUE;
    }
#endif

#ifdef LOG_LOCAL5
    if (strcmp("local5",facstr) == 0) {
        *facility = LOG_LOCAL5;
        return TRUE;
    }
#endif

#ifdef LOG_LOCAL6
    if (strcmp("local6",facstr) == 0) {
        *facility = LOG_LOCAL6;
        return TRUE;
    }
#endif

#ifdef LOG_LOCAL7
    if (strcmp("local7",facstr) == 0) {
        *facility = LOG_LOCAL7;
        return TRUE;
    }
#endif

    return FALSE;
}

static void logc_logger_file(
    const char     *domain,
    GLogLevelFlags  log_level,
    const char     *message,
    gpointer        user_data) {

    gsize           bw;
    char            timebuf[80];

    air_time_buf_print(timebuf, time(NULL), AIR_TIME_ISO8601);

    g_io_channel_write_chars(logc.logfile, "[", 1, &bw, NULL);
    g_io_channel_write_chars(logc.logfile, timebuf, -1, &bw, NULL);
    g_io_channel_write_chars(logc.logfile, "] ", 2, &bw, NULL);
    g_io_channel_write_chars(logc.logfile, message, -1, &bw, NULL);
    g_io_channel_write_chars(logc.logfile, "\n", 1, &bw, NULL);
    g_io_channel_flush(logc.logfile, NULL);
}

static void logc_logger_syslog(
    const char     *domain,
    GLogLevelFlags  log_level,
    const char     *message,
    gpointer        user_data) {

    syslog(logc_syslog_level(log_level), "%s", message);
}

static void logc_logger_null(
    const char     *domain,
    GLogLevelFlags  log_level,
    const char     *message,
    gpointer        user_data) {

    return;
}

gboolean logc_setup(
    GError          **err) {

    GLogLevelFlags  levels;
    int             facility;

    /* check for version flag */
    if (logc.opt_version) {
        logc_print_version();
        exit(0);
    }

    /* default to stderr, or user for forking daemon */
    if (logc.logspec == NULL) {
        if (daec_will_fork()) {
            logc.logspec = "user";
        } else {
            logc.logspec = "stderr";
        }
    }

    /* default to warning logging; handle verbose flag */
    if (logc.loglevel == NULL) {
        if (logc.opt_verbose) {
            logc.loglevel = "debug";
        } else {
            logc.loglevel = "warning";
        }
    }

    /* parse log level */
    if (!logc_parse_log_level(logc.loglevel, &levels)) {
        g_set_error(err, LOGC_ERROR_DOMAIN,
                    LOGC_ERROR_ARGUMENT,
                    "log level %s not recognized.",
                    logc.loglevel);
        return FALSE;
    }

    if (strcmp(logc.logspec, "stderr") == 0) {
        if (daec_will_fork()) {
            g_set_error(err, LOGC_ERROR_DOMAIN,
                        LOGC_ERROR_ARGUMENT,
                        "Can't log to stderr as daemon.");
            return FALSE;
        }

        /* set log file to stderr */
        logc.logfile = g_io_channel_unix_new(fileno(stderr));

        /* use file logger */
        g_log_set_handler(G_LOG_DOMAIN, levels, logc_logger_file, NULL);

    } else if (strchr(logc.logspec, '/')) {
        /* open log file */
        logc.logfile = g_io_channel_new_file(logc.logspec, "a", err);
        if (logc.logfile == NULL) return FALSE;

        /* use file logger */
        g_log_set_handler(G_LOG_DOMAIN, levels, logc_logger_file, NULL);

    } else {
        /* try to parse facility name */
        if (!logc_parse_syslog_facility(logc.logspec, &facility)) {
            g_set_error(err, LOGC_ERROR_DOMAIN,
                        LOGC_ERROR_ARGUMENT,
                        "syslog(3) facility %s not recognized.",
                        logc.logspec);
            return FALSE;
        }

        /* open log socket */
        openlog(logc.appname, LOG_CONS | LOG_PID, facility);

        /* use syslog logger */
        g_log_set_handler(G_LOG_DOMAIN, levels, logc_logger_syslog, NULL);
    }

    /* set default log handler to eat messages */
#if GLIB_CHECK_VERSION(2,6,0)
    g_log_set_default_handler(logc_logger_null, NULL);
#else
    /* GLib < 2.6 needs a little bit of help here */
    g_log_set_handler(G_LOG_DOMAIN, ~levels, logc_logger_null, NULL);
#endif
    g_message("%s starting", logc.appname);

    return TRUE;
}
