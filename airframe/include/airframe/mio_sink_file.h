/*
 ** mio_sink_file.h
 ** Multiple I/O regular file sink, by pattern.
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
 * MIO file sink initializers. Most applications should use the 
 * interface in mio_config.h to access these initializers.
 */

/* idem hack */
#ifndef _AIRFRAME_MIO_SINK_FILE_H_
#define _AIRFRAME_MIO_SINK_FILE_H_
#include <airframe/mio.h>

/**
 * File sink configuration context. Pass as the cfg argument to any file
 * sink initializer.
 */
typedef struct _MIOSinkFileConfig {
    /** 
     * Next serial number to assign to %S or %X pattern variable. 
     * Modified by sinks initialized by mio_sink_init_file_pattern(). 
     */
    uint32_t        next_serial;
} MIOSinkFileConfig;

/**
 * Initialize a file sink for writing to a single file. Fails over to 
 * mio_sink_init_stdout() if specifier is the special string "-".
 *
 * @param sink      pointer to MIOSink to initialize. This MIOSink will 
 *                  be overwritten.
 * @param spec      input specifier to initialize MIOSink with.
 *                  Must be a filename.
 * @param vsp_type  requested sink pointer type, or MIO_T_ANY for default.
 * @param cfg       pointer to configuration context. 
 *                  Must be a pointer to an MIOSinkFileConfig.
 * @param err       An error description pointer.
 * @return TRUE if the MIOSink was successfully initialized.
 */

gboolean mio_sink_init_file_single(
    MIOSink         *sink,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err);

/**
 * Initialize a file sink for writing to a multiple files based upon a 
 * pattern. Fails over to mio_sink_file_single() if specifier does not have 
 * any pattern variables.
 * 
 * The following pattern variables are supported:
 *
 * - %T timestamp at sink open in YYYYMMDDHHMMSS format
 * - %S serial number (from cfg) in decimal
 * - %X serial number (from cfg) in hex
 * - %d dirname of source active at sink open
 * - %s basename of source active at sink open
 * - %e extension of source active at sink open
 *
 * @param sink      pointer to MIOSink to initialize. This MIOSink will 
 *                  be overwritten.
 * @param spec      input specifier to initialize MIOSink with.
 *                  Must be a filename.
 * @param vsp_type  requested sink pointer type, or MIO_T_ANY for default.
 * @param cfg       pointer to configuration context. 
 *                  Must be a pointer to an MIOSinkFileConfig.
 * @param err       An error description pointer.
 * @return TRUE if the MIOSink was successfully initialized.
 */

gboolean mio_sink_init_file_pattern(
    MIOSink         *sink,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err);

/* end idem */
#endif
