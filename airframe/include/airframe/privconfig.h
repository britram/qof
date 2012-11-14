/*
 ** privconfig.c
 ** Generic privilege configuration support.
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2006-2011 Carnegie Mellon University. All Rights Reserved.
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
 * Airframe Privilege Configuration Support. Supplies privilege dropping 
 * for post-root initialization reduction of privileges (e.g. for live packet
 * capture applications) and the command line option processing necessary to 
 * use it. Use this when you want to drop privileges after doing one-time
 * setup as root.
 */

/* idem hack */
#ifndef _AIR_PRIVCONFIG_H_
#define _AIR_PRIVCONFIG_H_

#include <airframe/autoinc.h>
#include <airframe/airopt.h>

/** GError domain for privconfig errors */
#define PRIVC_ERROR_DOMAIN g_quark_from_string("airframePrivilegeError")
/** 
 * Privconfig setup error. Signifies that setup failed because of bad command
 * line options.
 */
#define PRIVC_ERROR_SETUP  1
/** 
 * Privilege drop error. 
 */
#define PRIVC_ERROR_FAILED 2
/** 
 * Couldn't drop privilege because privilege already dropped. 
 */
#define PRIVC_ERROR_ALREADY 3
/** 
 * Won't drop privilege because not running as root. 
 */
#define PRIVC_ERROR_NODROP  4

/**
 * Return an option group for privilege configuration. This option group defines 
 * two options: --become-user (-U) to become a specified user by name, 
 * and --become-group to additionally specify a group to become (otherwise,
 * drops privileges to the given user's default group.)
 * 
 * @param aoctx airframe option context
 * @return TRUE if successful, FALSE otherwise
 */
gboolean privc_add_option_group(AirOptionCtx *aoctx);

/**
 * Set up privilege configuration. Call this after parsing an options context 
 * including a GOptionGroup returned from privc_option_group(). This sets
 * up internal state used by the other privconfig calls.
 *
 * @param err an error description
 * @return TRUE on success, FALSE otherwise
 */

gboolean privc_setup(
    GError          **err);

/**
 * Determine if the user wants to drop privileges. Use this to determine
 * whether warn the user if the application will not call priv_become() due 
 * to some application-specific state.
 *  
 * @return TRUE if --become-user supplied on command line.
 */
gboolean privc_configured();

/**
 * Drop privileges if necessary. Returns TRUE if not running as root. Returns
 * FALSE if running as root with no --become-user option with 
 * PRIVC_ERROR_NODROP, or if privc_become() was already called succsssfully
 * with PRIVC_ERROR_ALREADY. If for some reason a required privilege drop
 * fails, returns FALSE with PRIVC_ERROR_FAILED.
 *
 * @param err an error description
 * @return TRUE on success, FALSE otherwise
 */
gboolean privc_become(
    GError          **err);

/* end idem */
#endif
