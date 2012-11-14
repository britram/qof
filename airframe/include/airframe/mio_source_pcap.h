/*
 ** mio_source_pcap.c
 ** Multiple I/O pcap source, from files, directories, or live capture
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
 * MIO libpcap source initializers. Most applications should use the 
 * interface in mio_config.h to access these initializers.
 */

/* idem hack */
#ifndef _AIRFRAME_MIO_SOURCE_PCAP_H_
#define _AIRFRAME_MIO_SOURCE_PCAP_H_
#include <airframe/mio.h>
#include <airframe/mio_source_file.h>

/** 
 * Convenience macro to get a source's currently open pcap context.
 * Only valid if the source's vsp_type is MIO_T_PCAP.
 */
#define mio_pcap(_s_) ((pcap_t *)(_s_)->vsp)

/**
 * libpcap dumpfile source configuration context. Pass as the cfg argument to 
 * any pcap file source initializer.
 */
typedef struct _MIOSourcePCapFileConfig {
    /** File source configuration context; used for handling dumpfiles. */
    MIOSourceFileConfig filecfg;
    /** BPF filter expression to apply when reading dumpfiles. */
    char                *filter;
} MIOSourcePCapFileConfig;

/**
 * libpcap live source configuration context. Pass as the cfg argument to 
 * mio_source_init_pcap_live().
 */
typedef struct _MIOSourcePCapLiveConfig {
    /** Live capture length in octets. */
    uint32_t            snaplen;
    /** Live capture timeout in milliseconds. */
    uint32_t            timeout;
    /** BPF filter expression to apply when capturing packets. */    
    char                *filter;
} MIOSourcePCapLiveConfig;

/**
 * Initialize a pcap source for reading every libpcap dumpfile from a 
 * specified directory.
 *
 * @param source    pointer to MIOSource to initialize. This MIOSource will 
 *                  be overwritten.
 * @param spec      input specifier to initialize MIOSource with.
 *                  Must be the pathname of an accessible directory.
 * @param vsp_type  requested source pointer type, or MIO_T_ANY for default.
 * @param cfg       pointer to configuration context. 
 *                  Must be a pointer to an MIOSourcePcapFileConfig structure.
 * @param err       An error description pointer.
 * @return TRUE if the MIOSource was successfully initialized.
 */

gboolean mio_source_init_pcap_dir(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err);

/**
 * Initialize a pcap source for reading every libpcap dumpfile from a 
 * specified glob(3) expression. Fails over to mio_source_init_pcap_single() 
 * if the specifier contains no glob expression characters.
 *
 * @param source    pointer to MIOSource to initialize. This MIOSource will 
 *                  be overwritten.
 * @param spec      input specifier to initialize MIOSource with.
 *                  Must be a glob expression.
 * @param vsp_type  requested source pointer type, or MIO_T_ANY for default.
 * @param cfg       pointer to configuration context. 
 *                  Must be a pointer to an MIOSourcePcapFileConfig structure.
 * @param err       An error description pointer.
 * @return TRUE if the MIOSource was successfully initialized.
 */

gboolean mio_source_init_pcap_glob(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err);

/**
 * Initialize a pcap source for a single libpcap dumpfile. Fails over to 
 * mio_source_init_pcap_stdin() if specifier is the special string "-".
 *
 * @param source    pointer to MIOSource to initialize. This MIOSource will 
 *                  be overwritten.
 * @param spec      input specifier to initialize MIOSource with.
 *                  Must be a filename.
 * @param vsp_type  requested source pointer type, or MIO_T_ANY for default.
 * @param cfg       pointer to configuration context. 
 *                  Must be a pointer to an MIOSourcePcapFileConfig structure.
 * @param err       An error description pointer.
 * @return TRUE if the MIOSource was successfully initialized.
 */
    
gboolean mio_source_init_pcap_single(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err);

/**
 * Initialize a pcap source for a single libpcap dumpfile read from standard 
 * input.
 *
 * @param source    pointer to MIOSource to initialize. This MIOSource will 
 *                  be overwritten.
 * @param spec      input specifier to initialize MIOSource with.
 *                  Must be the string "-".
 * @param vsp_type  requested source pointer type, or MIO_T_ANY for default.
 * @param cfg       pointer to configuration context. 
 *                  Must be a pointer to an MIOSourcePcapFileConfig structure.
 * @param err       An error description pointer.
 * @return TRUE if the MIOSource was successfully initialized.
 */

gboolean mio_source_init_pcap_stdin(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err);

/**
 * Initialize a pcap source for live capture from an interface using libpcap.
 * Depending on the operating system and configuration, this may require
 * special privileges. 
 *
 * @param source    pointer to MIOSource to initialize. This MIOSource will 
 *                  be overwritten.
 * @param spec      input specifier to initialize MIOSource with.
 *                  Must be a valid libpcap interface name.
 * @param vsp_type  requested source pointer type, or MIO_T_ANY for default.
 * @param cfg       pointer to configuration context. 
 *                  Must be a pointer to an MIOSourcePcapLiveConfig structure.
 * @param err       An error description pointer.
 * @return TRUE if the MIOSource was successfully initialized.
 */

gboolean mio_source_init_pcap_live(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err);
    
/* end idem */
#endif
