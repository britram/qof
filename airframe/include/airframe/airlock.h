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

/**
 * @file 
 *
 * Airframe lockfile interface. Used to acquire lockfiles compatible with 
 * the Airframe filedaemon.
 */
 
/* idem hack */
#ifndef _AIR_AIRLOCK_H_
#define _AIR_AIRLOCK_H_

#include <airframe/autoinc.h>

/** GError domain for lock errors */
#define LOCK_ERROR_DOMAIN g_quark_from_string("airframeLockError")
/** 
 * A lock could not be acquired.
 */
#define LOCK_ERROR_LOCK  1

/** 
 * A lock structure. Must be maintained by a caller from lock acquisition
 * to release. Should be initialized by AIR_LOCK_INIT or memset(0) or bzero(). 
 */
typedef struct _AirLock {
    /** Path to .lock file */
    GString     *lpath;
    /** File descriptor of open .lock file */
    int         lfd;
    /** TRUE if this lock is held, and lpath and lfd are valid. */
    gboolean    held;
} AirLock;

/** Convenience initializer for AirLock structures */
#define AIR_LOCK_INIT { NULL, 0, FALSE }

/**
 * Acquire a lock. Creates a lockfile and returns TRUE if the lockfile was
 * created (and is now held). 
 *
 * @param lock AirLock structure to store lockfile information in.
 * @param path path of file to lock (without .lock extension).
 * @param err an error descriptor
 * @return TRUE if lockfile created, FALSE if lock not available
 */
gboolean air_lock_acquire(
    AirLock     *lock,
    const char  *path,
    GError      **err);

/**
 * Release an acquired lock.
 * 
 * @param lock AirLock structure filled in by air_lock_acquire()
 */
void air_lock_release(
    AirLock     *lock);

/**
 * Free storage used by an AirLock structure. Does not free the structure
 * itself.
 *
 * @param lock AirLock to free
 */
void air_lock_cleanup(
    AirLock     *lock);

/* end idem */
#endif
