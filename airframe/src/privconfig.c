/*
 ** privconfig.c
 ** Generic privilege configuration support.
 **
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

#define _AIRFRAME_SOURCE_
#include <airframe/privconfig.h>

static char *RCSID __attribute__ ((unused)) = 
    "$Id: daeconfig.c 3509 2006-04-21 14:09:13Z bht $";

static char *opt_user = NULL;
static char *opt_group = NULL;

static uid_t new_user = 0;
static gid_t new_group = 0;

static gboolean did_become = FALSE;

#define THE_LAME_80COL_FORMATTER_STRING "\n\t\t\t\t"

AirOptionEntry privc_optentries[] = {
    AF_OPTION( "become-user", 'U', 0, AF_OPT_TYPE_STRING, &opt_user,
              THE_LAME_80COL_FORMATTER_STRING"Become user after setup if "
	       "started as root", NULL ),
    AF_OPTION( "become-group", (char)0, 0, AF_OPT_TYPE_STRING, &opt_group,
               THE_LAME_80COL_FORMATTER_STRING"Become group after setup if "
	       "started as root", NULL ),
    AF_OPTION_END
};

gboolean privc_add_option_group(
    AirOptionCtx *aoctx)
{
    g_assert(aoctx != NULL);

    air_option_context_add_group(aoctx, "privilege", "Privilege Options:",
                                 THE_LAME_80COL_FORMATTER_STRING"Show help "
				 "for privilege options", privc_optentries);

    return TRUE;
}

gboolean privc_setup(
    GError          **err) 
{
    struct passwd   *pwe = NULL;
    struct group    *gre = NULL;

    if (geteuid() == 0) {
        /* We're root. Parse user and group names. */
        if (opt_user) {
            if (!(pwe = getpwnam(opt_user))) {
                g_set_error(err, PRIVC_ERROR_DOMAIN, PRIVC_ERROR_SETUP,
                            "Cannot become user %s: %s.",
                            opt_user, strerror(errno));
                return FALSE;
            }
            
            /* By default, become new user's user and group. */
            new_user = pwe->pw_uid;
            new_group = pwe->pw_gid;
            if (opt_group) {
                if (!(gre = getgrnam(opt_group))) {
                    g_set_error(err, PRIVC_ERROR_DOMAIN, PRIVC_ERROR_SETUP,
                                "Cannot become group %s: %s.",
                                opt_group, strerror(errno));
                    return FALSE;
                }
            
                /* Override new group if set */
                new_group = gre->gr_gid;
            }
        }
    } else {
        /* We're not root. If we have options, the user is confused, and
           we should straighten him out by killing the process. */
        if (opt_user) {
            g_set_error(err, PRIVC_ERROR_DOMAIN, PRIVC_ERROR_SETUP,
                        "Cannot become user %s: not root.",
                        opt_user);
            return FALSE;
        }
        if (opt_group) {
            g_set_error(err, PRIVC_ERROR_DOMAIN, PRIVC_ERROR_SETUP,
                        "Cannot become group %s: not root.",
                        opt_user);
            return FALSE;
        }
    }
    
    /* All done. */
    return TRUE;
}

gboolean privc_configured()
{
    return (new_user) ? TRUE : FALSE;
}

gboolean privc_become(
    GError          **err)
{
    /* Die if we've already become */
    if (did_become) {
        g_set_error(err, PRIVC_ERROR_DOMAIN, PRIVC_ERROR_ALREADY,
                    "not dropping privileges, already did so");
        return FALSE;
    }

    /* Short circuit if we're not root */
    if (geteuid() != 0) return TRUE;

    /* Allow app to warn if not dropping */
    if (new_user == 0) {
        g_set_error(err, PRIVC_ERROR_DOMAIN, PRIVC_ERROR_NODROP,
                    "not dropping privileges (use --become-user to do so)");
        return FALSE;
    }
    
    /* Okay. Do the drop. */
    
    /* Drop ancillary group privileges while we're still root */
    if (setgroups(1, &new_group) < 0) {
        g_set_error(err, PRIVC_ERROR_DOMAIN, PRIVC_ERROR_FAILED,
                    "couldn't drop ancillary groups: %s", strerror(errno));
        return FALSE;
    }
    
#if LINUX_PRIVHACK
    /* Change to group */
    if (setregid(new_group, new_group) < 0) {
        g_set_error(err, PRIVC_ERROR_DOMAIN, PRIVC_ERROR_FAILED,
                    "couldn't become group %u: %s", new_group, strerror(errno));
        return FALSE;
    }
        
    /* Lose root privileges */
    if (setreuid(new_user, new_user) < 0) {
        g_set_error(err, PRIVC_ERROR_DOMAIN, PRIVC_ERROR_FAILED,
                    "couldn't become user %u: %s", new_user, strerror(errno));
        return FALSE;
    }
#else
    /* Change to group */
    if (setgid(new_group) < 0) {
        g_set_error(err, PRIVC_ERROR_DOMAIN, PRIVC_ERROR_FAILED,
                    "couldn't become group %u: %s", new_group, strerror(errno));
        return FALSE;
    }
        
    /* Lose root privileges */
    if (setuid(new_user) < 0) {
        g_set_error(err, PRIVC_ERROR_DOMAIN, PRIVC_ERROR_FAILED,
                    "couldn't become user %u: %s", new_user, strerror(errno));
        return FALSE;
    }
#endif

    /* All done. */
    did_become = TRUE;
    return TRUE;
}
