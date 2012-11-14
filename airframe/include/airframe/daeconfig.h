/*
 ** daeconfig.h
 ** Generic daemon configuration support
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2005-2011 Carnegie Mellon University. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell
 **          Tony Cebzanov <tonyc@cert.org>
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

/**
 * @file
 *
 * Airframe Daemon Configuration Support. Supplies automatic daemonization 
 * and the command line option processing necessary to use it. Use this when
 * your application can run as a daemon and you want to give your users control 
 * over whether it does via the command line.
 */

/* idem hack */
#ifndef _AIR_DAECONFIG_H_
#define _AIR_DAECONFIG_H_

#include <airframe/autoinc.h>
#include <airframe/airopt.h>

/** GError domain for daeconfig errors */
#define DAEC_ERROR_DOMAIN g_quark_from_string("airframeDaemonError")
/** 
 * Daeconfig setup error. Signifies that daemonization failed due to an
 * underlying operating system error.
 */
#define DAEC_ERROR_SETUP  1

/**
 * Set up daemon configuration. Call this after parsing an options context 
 * including a GOptionGroup returned from daec_option_group(). This sets
 * up internal state used by the other daeconfig calls and daemonizes the 
 * application, if necessary.
 *
 * @param err an error description
 * @return TRUE on success, FALSE otherwise
 */
 
gboolean daec_setup(
    GError          **err);

/**
 * Add an option group for daemon configuration to the given optoin context.
 * This option group defines two options: --daemon (-d) to become a daemon, and
 * --foreground to run in daemon mode without forking.
 *
 * @param aoctx airframe option context
 * @return TRUE if successful, FALSE otherwise
 */
gboolean daec_add_option_group(AirOptionCtx *aoctx);

/**
 * Return daemon mode state. Returns true if --daemon was passed in on the 
 * command line, regardless of whether --foreground was also present. If an 
 * application's logic is different for daemon and non-daemon mode, the 
 * application should use this call to determine which mode to run in.
 *
 * @return TRUE if in daemon mode, FALSE otherwise. 
 */

gboolean daec_is_daemon();

/**
 * Return future fork state. Returns true if --daemon and not --foreground. Use 
 * this call to determine whether a call to daec_setup() will cause the 
 * application for fork to the background. This is primarily designed for 
 * interoperation with logconfig, which must know whether daeconfig will 
 * fork without requiring said fork to occur before logging is set up.
*
 * @return TRUE if subsequent call to daec_setup() will fork, FALSE otherwise.
 */
 
gboolean daec_will_fork();

/**
 * Return forked state. Returns true if a prior call to daec_setup() caused 
 * the application to fork to the background. 
 *
 * @return TRUE if the daemon has forked, FALSE otherwise
 */
 
gboolean daec_did_fork();

/**
 * Return quit flag state. Returns FALSE until daec_quit() has been called, 
 * then returns TRUE. Provided as a convenience, so applications don't have 
 * to track their own quit flag.
 *
 * @return TRUE if daec_quit() has been called.
 */
 
gboolean daec_did_quit();

/**
 * Set the quit flag.
 */
 
void daec_quit();

/* end idem */
#endif
