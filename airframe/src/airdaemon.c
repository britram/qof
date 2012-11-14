/*
** airdaemon.c
** Keeps a child process running.
**
** ------------------------------------------------------------------------
** Copyright (C) 2007-2011 Carnegie Mellon University. All Rights Reserved.
** ------------------------------------------------------------------------
** Authors: Tony Cebzanov <toyc@cert.org>
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
#include <airframe/autoinc.h>
#include <airframe/airopt.h>
#include <airframe/logconfig.h>

uint32_t     ad_retry_min        = 30;
uint32_t     ad_retry_max        = 0;
gboolean     ad_nodaemon         = FALSE;
pid_t        ad_pid              = 0;
char        *ad_pidfile          = NULL;
pid_t        ad_cpid              = 0;
char        *ad_cpidfile          = NULL;


AirOptionEntry ad_options[]  = {
    AF_OPTION( "retry", 'r', 0, AF_OPT_TYPE_INT, &ad_retry_min,
               "Retry delay in seconds", "sec" ),
    AF_OPTION( "retry-max", 'R', 0, AF_OPT_TYPE_INT, &ad_retry_max,
               "Retry delay maximum in seconds", NULL ),
    AF_OPTION( "pidfile", 'P', 0, AF_OPT_TYPE_STRING, &ad_cpidfile,
               "A filename to write the child process pid to", NULL ),
    AF_OPTION( "airdaemon-pidfile", 'A', 0, AF_OPT_TYPE_STRING, &ad_pidfile,
               "A filename to write airdaemon's pid to", NULL ),
    AF_OPTION( "no-daemon", (char)0, 0, AF_OPT_TYPE_NONE, &ad_nodaemon,
               "do not daemonize", NULL ),
    AF_OPTION_END
};

typedef struct _ad_child_data {
    GMainLoop       *loop;
    gboolean        *done;
} ad_child_data_t;

static void parse_options(
    int              *argc,
    char            **argv[]) {

    AirOptionCtx    *aoctx = NULL;

    aoctx = air_option_context_new("", argc, argv, ad_options);
    logc_add_option_group(aoctx, "airdaemon", VERSION);

    air_option_context_set_help_enabled(aoctx);

    air_option_context_parse(aoctx);
}

static void on_child_exit(
    GPid     child_pid,
    gint     status,
    gpointer data)
{
    GMainLoop *loop = ((ad_child_data_t *)data)->loop;
    gboolean  *done = ((ad_child_data_t *)data)->done;


    g_message("pid %lu exited with status %d", (gulong) child_pid, status);

#ifdef G_OS_UNIX
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == EXIT_SUCCESS) {
            g_debug("pid %lu returned success", (gulong) child_pid);
            *done = TRUE;
        } else {
            g_warning("pid %lu returned error status %d", (gulong) child_pid,
                      WEXITSTATUS(status));

        }
    } else if (WIFSIGNALED(status)) {
        g_critical("pid %lu terminated with signal %d\n",
                   (gulong) child_pid, WTERMSIG(status));

    } else {
        g_critical("pid %lu terminated", (gulong) child_pid);

    }
#endif /* G_OS_UNIX */
    g_spawn_close_pid(child_pid);
    g_main_loop_quit(loop);
}


gboolean daemonize(
    void)
{

    /* fork */
    if (fork()) exit(0);

    /* dissociate from controlling terminal */
    if (setsid() < 0) {
        g_critical("setsid() failed: %s", strerror(errno));
        return FALSE;
    }

    /* redirect stdio */
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    ad_pid = getpid();
    if (ad_pidfile) {
        FILE *pidfile = fopen(ad_pidfile,"w");
        if (!pidfile) {
            g_critical("could not write pidfile");
            goto end;
        }
        fprintf(pidfile, "%d\n", ad_pid);
        fclose(pidfile);
    }
  end:

    return TRUE;
}


int main(
    int              argc,
    char            *argv[])
{

    int                      i;
    gboolean                 done=FALSE;
    GString                 *cmd;
    GError                  *err = NULL;
    uint32_t                 delay;
    GTimer                  *uptimer  = NULL;
    gdouble                  elapsed_time;

    GMainLoop *loop;

    GPtrArray    *child_args     = NULL;

    /* parse options */
    parse_options(&argc, &argv);

    /* set up logging */
    if (!logc_setup(&err)) {
        air_opterr("%s", err->message);
    }

    if (ad_retry_max && (ad_retry_min > ad_retry_max) ) {
        air_opterr("--retry value (%d) cannot exceed --retry-max value (%d) ",
                   ad_retry_min,
                   ad_retry_max);
    }
    delay = ad_retry_min;

    child_args = g_ptr_array_sized_new(64);
    for (i=1; i < argc; i++) {
        /* Double dash indicates end of airdaemon's arguments */
        if (!strncmp(argv[i], "--", strlen(argv[i])) )
            continue;
        g_ptr_array_add(child_args, g_strdup(argv[i]));
    }
    g_ptr_array_add(child_args, NULL);

    cmd  = g_string_new("");

    loop = g_main_loop_new(NULL, FALSE);

    /* Options check out; daemonize */
    if (!ad_nodaemon) {
        if (!daemonize()) {
            goto end;
        }
    }

    uptimer = g_timer_new();

    while (!done) {
        GPid            child_pid;
        char          **child_envp            = {NULL};
        GError         *child_err             = NULL;
        ad_child_data_t child_data;

        if (!g_spawn_async_with_pipes(".",
                                      (gchar **) child_args->pdata,
                                      child_envp,
                                      G_SPAWN_SEARCH_PATH |
                                      G_SPAWN_DO_NOT_REAP_CHILD,
                                      NULL,
                                      NULL,
                                      &child_pid,
                                      NULL,
                                      NULL,
                                      NULL,
                                      &child_err))
        {
            g_error("error spawning process: %s",
                    (child_err && child_err->message ? child_err->
                     message : "unknown error"));
        }

        g_timer_start(uptimer);

        /* Write child pid if requested */
        if (ad_cpidfile) {
            FILE *cpidfile = fopen(ad_cpidfile,"w");
            if (!cpidfile) {
                g_critical("could not write pidfile");
                goto end;
            }
            fprintf(cpidfile, "%d\n", child_pid);
            fclose(cpidfile);
        }

        /* Watch for process exit status */
        child_data.loop = loop;
        child_data.done = &done;

        g_child_watch_add(child_pid, on_child_exit, &child_data);
        g_main_loop_run(loop);

        g_timer_stop(uptimer);
        elapsed_time = g_timer_elapsed(uptimer, NULL);

        if (done) {
            g_debug("done");
        } else {
            if (ad_retry_max && (elapsed_time >= ad_retry_min) ) {
                g_debug("child survived for %fs, resetting delay", elapsed_time);
                delay = ad_retry_min;
            }
            g_debug("child exited abnormally, sleeping for %d seconds", delay);
            sleep(delay);
            if (ad_retry_max) {
                if (2 * delay <= ad_retry_max)
                    delay *= 2;
                else
                    delay = ad_retry_max;
            }

        }
    }

  end:

    return 0;
}

