/*
** airlock.c
** Airframe lockfile interface
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
#include <airframe/airlock.h>

static char *RCSID __attribute__((unused)) =
    "$Id: airutil.c 4728 2006-08-30 14:41:01Z bht $";

gboolean air_lock_acquire(
    AirLock     *lock,
    const char  *path,
    GError      **err)
{

    /* Lazily create scratch path */
    if (!lock->lpath) {
        lock->lpath = g_string_new("");
    }

    /* Generate lock path */
    g_string_printf(lock->lpath, "%s.lock", path);
    
    /* Open lock file */
    lock->lfd = open(lock->lpath->str, O_WRONLY | O_CREAT | O_EXCL, 0664);
    if (lock->lfd < 0) {
        g_set_error(err, LOCK_ERROR_DOMAIN, LOCK_ERROR_LOCK,
                    "Cannot lock file %s: %s",
                    path, strerror(errno));
        unlink(lock->lpath->str);
        return FALSE;
    }
    
    /* Note lock held */
    lock->held = TRUE;
    
    return TRUE;
}
    
void air_lock_release(
    AirLock     *lock)
{
    /* Lock release is no-op if lock not held */
    if (!lock->held) {
        return;
    }
    
    /* Verify lockfile still exists */
    if (!g_file_test(lock->lpath->str, G_FILE_TEST_IS_REGULAR)) {
        g_warning("Lock collision warning: %s missing", 
                  lock->lpath->str);
    }
 
    /* Close and unlink lockfile */
    close(lock->lfd);
    unlink(lock->lpath->str);
    
    /* clean up the lock */
    lock->held = FALSE;
}

void air_lock_cleanup(
    AirLock     *lock)
{
    if (lock->lpath) {
        g_string_free(lock->lpath, TRUE);
    }
}
