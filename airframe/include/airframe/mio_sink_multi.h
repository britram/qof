/*
 ** mio_sink_multi.h
 ** Multiple I/O multisink, for output fanout.
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

/**
 * @file
 *
 * MIO multisink initializer and utilities. 
 */

/* idem hack */
#ifndef _AIRFRAME_MIO_SINK_MULTI_H_
#define _AIRFRAME_MIO_SINK_MULTI_H_
#include <airframe/mio.h>

/**
 * Initialize a multisink for writing to multiple subordinate sinks. A 
 * multisink simply distributes its operations (next, close, free) among 
 * its subordinates. This function creates a multisink with all of its 
 * subordinate sinks zeroed - after initializing, each subordinate sink must
 * in turn be initialized by a specific sink initializer.
 *
 *
 * @param sink      pointer to MIOSink to initialize. This MIOSink will 
 *                  be overwritten.
 * @param spec      input specifier to initialize MIOSource with.
 *                  Ignored; may be NULL.
 * @param vsp_type  requested sink pointer type, or MIO_T_ANY for default.
 *                  Must be ANY or MULTISINK.
 * @param cfg       Number of subordinate sinks to allocate
 *                  cast to a void pointer using GUINT_TO_POINTER.
 * @param err       An error description pointer.
 * @return TRUE if the MIOSink was successfully initialized.
 */
gboolean mio_sink_init_multi(
    MIOSink         *sink,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err);

/**
 * Convenience macro to retrieve the subordinate sink count for a 
 * given multisink.
 */
#define mio_smc(_s_) (GPOINTER_TO_UINT((_s_)->cfg))
 
/**
 * Convenience macro to access a given subordinate sink by index for a 
 * given multisink. Evaluates to a structure; use the address operator to
 * get a pointer to the subordinate sink.
 */
#define mio_smn(_s_, _n_) (((MIOSink *)(_s_)->vsp)[(_n_)])

/* end idem */
#endif
