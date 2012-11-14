/*
 ** daeconfig.c
 ** Generic daemon configuration support
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
#include <airframe/daeconfig.h>
#include <airframe/airopt.h>

static char *RCSID __attribute__ ((unused)) = 
    "$Id: daeconfig.c 16936 2011-04-22 14:57:28Z ecoff_svn $";

static gboolean opt_daemon = FALSE;
static gboolean opt_fg = FALSE;

static gboolean did_fork = FALSE;

static gboolean daemon_quit = FALSE;

AirOptionEntry daec_optentries[] = {
    AF_OPTION( "daemon", 'd', 0, AF_OPT_TYPE_NONE, &opt_daemon,
               "Become daemon", NULL ),
    AF_OPTION( "foreground", (char)0, 0, AF_OPT_TYPE_NONE, &opt_fg,
               "Do not fork to background in daemon mode", NULL ),
    AF_OPTION_END
};

gboolean daec_add_option_group(AirOptionCtx *aoctx)
{
    g_assert(aoctx != NULL);
    
    air_option_context_add_group(aoctx, "daemon", "Daemon options:",
                                 "Show help for daemon options", 
                                 daec_optentries);
    
    return TRUE;    
}

gboolean daec_is_daemon() {
    return opt_daemon;
}

gboolean daec_did_fork() {
    return did_fork;
}

gboolean daec_will_fork() {
    return opt_daemon ? (opt_fg ? 0 : 1 ) : 0;
}

void daec_quit() {
    ++daemon_quit;
}

gboolean daec_did_quit() {
    return daemon_quit;
}

gboolean daec_setup(
    GError              **err)
{
    struct sigaction sa, osa;

    /* fork if necessary */
    if (daec_will_fork()) {

        /* fork */
        if (fork()) exit(0);

        /* dissociate from controlling terminal */
        if (setsid() < 0) {
            g_set_error(err, DAEC_ERROR_DOMAIN, DAEC_ERROR_SETUP,
                "setsid() failed: %s", strerror(errno));
            return FALSE;
        }

        /* redirect stdio */
        freopen("/dev/null", "r", stdin);
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);

        /* we forked */
        did_fork = TRUE;
    }
    
    /* install quit flag handlers */
    sa.sa_handler = daec_quit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT,&sa,&osa)) {
        g_set_error(err, DAEC_ERROR_DOMAIN, DAEC_ERROR_SETUP,
            "sigaction(SIGINT) failed: %s", strerror(errno));
        return FALSE;
    }

    sa.sa_handler = daec_quit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGTERM,&sa,&osa)) {
        g_set_error(err, DAEC_ERROR_DOMAIN, DAEC_ERROR_SETUP,
            "sigaction(SIGTERM) failed: %s", strerror(errno));
        return FALSE;
    }

    return TRUE;
}

