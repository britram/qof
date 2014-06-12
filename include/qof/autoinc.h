/*
 ** autoinc.h
 ** Autotools-happy standard library include file
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2005-2012 Carnegie Mellon University. All Rights Reserved.
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

/** @file
 *  Convenience include file for libyaf.
 */

#ifndef _YAF_AUTOINC_H_
#define _YAF_AUTOINC_H_

#ifdef _YAF_SOURCE_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#endif

#include <stdio.h>

#if     HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#if     STDC_HEADERS
#  include <stdlib.h>
#  include <stddef.h>
#else
#  if   HAVE_STDLIB_H
#    include <stdlib.h>
#  endif
#  if   HAVE_MALLOC_H
#    include <malloc.h>
#  endif
#endif

#if     HAVE_STRING_H
#  if   !STDC_HEADERS && HAVE_MEMORY_H
#    include <memory.h>
#  endif
#  include <string.h>
#endif

#if     HAVE_INTTYPES_H
#  include <inttypes.h>
#else
#  if   HAVE_STDINT_H
#    include <stdint.h>
#  endif
#endif

#if     HAVE_ERRNO_H
#  include <errno.h>
#endif

#if     HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#if     HAVE_SIGNAL_H
#  include <signal.h>
#endif

#if     HAVE_SIGNAL_H
#  include <signal.h>
#endif

#if     HAVE_GLOB_H
#  include <glob.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <glib.h>
#if GLIB_CHECK_VERSION(2,6,0)
#  include <glib/gstdio.h>
#else
#define g_debug(...)    g_log (G_LOG_DOMAIN,         \
                               G_LOG_LEVEL_DEBUG,    \
                               __VA_ARGS__)
#endif


#ifdef _YAF_SOURCE_
#if YAF_DISABLE_SLABALLOC || !GLIB_CHECK_VERSION(2,10,0)
#define yg_slice_new0(_t_) g_new0(_t_, 1)
#define yg_slice_alloc0(_s_) g_malloc0(_s_)
#define yg_slice_alloc(_s_) g_malloc(_s_)
#define yg_slice_free(_t_, _p_) g_free(_p_)
#define yg_slice_free1(_s_, _p_) g_free(_p_)
#else
#define yg_slice_new0(_t_) g_slice_new0(_t_)
#define yg_slice_alloc0(_s_) g_slice_alloc0(_s_)
#define yg_slice_alloc(_s_) g_slice_alloc(_s_)
#define yg_slice_free(_t_, _p_) g_slice_free(_t_, _p_)
#define yg_slice_free1(_s_, _p_) g_slice_free1(_s_, _p_)
#endif
#endif

/*
#ifdef _YAF_SOURCE_
#if YAF_DISABLE_SLABALLOC || !defined g_slice_new0
#define yg_slice_new0(_t_) g_new0(_t_, 1)
#define yg_slice_alloc0(_s_) g_malloc0(_s_)
#define yg_slice_alloc(_s_) g_malloc(_s_)
#else
#define yg_slice_alloc0(_s_) g_slice_alloc0(_s_)
#define yg_slice_new0(_t_) g_slice_new0(_t_)
#define yg_slice_alloc(_s_) g_slice_alloc(_s_)
#endif

#if YAF_DISABLE_SLABALLOC || !defined g_slice_free
#define yg_slice_free(_t_, _p_) g_free(_p_)
#define yg_slice_free1(_s_, _p_) g_free(_p_)
#else
#define yg_slice_free(_t_, _p_) g_slice_free(_t_, _p_)
#define yg_slice_free1(_s_, _p_) g_slice_free1(_s_, _p_)
#endif
#endif
*/

/** the following PRI* macros code was taken from
silk_config.h */
/** PRI* macros for printing */
#if !defined(PRIu32)
/* Assume we either get them all or get none of them. */
#  define PRId32 "d"
#  define PRIi32 "i"
#  define PRIo32 "o"
#  define PRIu32 "u"
#  define PRIx32 "x"
#  define PRIX32 "X"

#  define PRId16 PRId32
#  define PRIi16 PRIi32
#  define PRIo16 PRIo32
#  define PRIu16 PRIu32
#  define PRIx16 PRIx32
#  define PRIX16 PRIX32

#  define PRId8  PRId32
#  define PRIi8  PRIi32
#  define PRIo8  PRIo32
#  define PRIu8  PRIu32
#  define PRIx8  PRIx32
#  define PRIX8  PRIX32
#endif /* !defined(PRIU32) */
#if !defined(PRIu64)
#  if (SIZEOF_LONG >= 8)
#    define PRId64 "l" PRId32
#    define PRIi64 "l" PRIi32
#    define PRIo64 "l" PRIo32
#    define PRIu64 "l" PRIu32
#    define PRIx64 "l" PRIx32
#    define PRIX64 "l" PRIX32
#  else
#    define PRId64 "ll" PRId32
#    define PRIi64 "ll" PRIi32
#    define PRIo64 "ll" PRIo32
#    define PRIu64 "ll" PRIu32
#    define PRIx64 "ll" PRIx32
#    define PRIX64 "ll" PRIX32
#  endif
#endif /* !defined(PRIu64) */

/** this UNUSED macro is also stolen from silk_config.h */
#ifdef __GNUC__
#define UNUSED(var) /*@unused@*/ var __attribute__((__unused__))
#else
#define UNUSED(var) /*@unused@*/ var
#endif


#ifdef __CYGWIN__
const char * yfGetCygwinConfDir (void);
#endif

#endif
