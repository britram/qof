/*
 ** logconfig.h
 ** Generic glib-based logging configuration support
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
 * Airframe Logging Configuration Support. Supplies glib log routing to 
 * standard error, file output, and the UNIX syslog facility, and the command 
 * line option processing necessary to use it. Integrates with daeconfig to 
 * ensure proper use of standard error, and to default to standard error or 
 * syslog as appropriate. Use this when your application uses glib logging and 
 * you want to give your users control over where to route logging information 
 * via the command line.
 */

/* idem hack */
#ifndef _AIR_LOGCONFIG_H_
#define _AIR_LOGCONFIG_H_

#include <airframe/autoinc.h>
#include <airframe/airopt.h>

/** GError domain for logconfig errors */
#define LOGC_ERROR_DOMAIN (g_quark_from_string("airframeLogError"))
/** 
 * Logconfig argument error. The user passed in an illegal command-line 
 * argument.
 */
#define LOGC_ERROR_ARGUMENT 1

/**
 * Add an option group for logging configuration to the given option context.
 * This option group defines four options: --log (-l) to specify a logging
 * destination, --loglevel (-L)  to specify the minimum severity of logged
 * messages, --verbose (-v) which is a shortcut for --loglevel debug, and
 * --version (-V) which will print version information and exit the application.
 * 
 * @param aoctx airframe option context
 * @param appname application name to display
 * @param version application version string
 * @return TRUE if successful, FALSE otherwise
 */
gboolean logc_add_option_group(
    AirOptionCtx   *aoctx,
    const char     *appname,
    const char     *version);

/**
 * Set up log routing. Call this after parsing an options context including a
 * GOptionGroup returned from logc_option_group(). This sets up log routing 
 * using logconfig; subsequent glib logging calls will be routed as specified
 * by the user.
 *
 * By default, if the application will fork to the background logging is 
 * routed to standard error; otherwise, it is routed to the "user" syslog 
 * facility. In either case, the default loglevel is warning.
 *
 * @param err an error description
 * @return TRUE on success, FALSE otherwise.
 */
 
gboolean logc_setup(
    GError          **err);

/* end idem */
#endif
