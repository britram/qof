/*
 ** mio_config.h
 ** Multiple I/O common command-line processing convenience module
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
 * Airframe Multiple I/O Configuration Support. Supplies command-line 
 * processing and configuration of MIOSource and MIOSink instances for 
 * MIO-based applications.
 *
 * Applications use mio_config by describing the source and sink types they
 * support via a set of flags, passing these flags to mio_option_group to get
 * an option group for GOption-based processing; then, after the command line
 * has been parsed, the application calls mio_config_source() and 
 * mio_config_sink() to create an appropriate source and sink, respectively. 
 */

/* idem hack */
#ifndef _AIRFRAME_MIO_CONFIG_H_
#define _AIRFRAME_MIO_CONFIG_H_
#include <airframe/mio.h>
#include <airframe/airopt.h>

/** Mask covering input flag bits; used internally. */
#define MIO_F_CLI_INMASK        0x0000007F 
/** Enable configuration of file, glob, and standard input */
#define MIO_F_CLI_FILE_IN       0x00000001
/** Enable configuration of file input from directory (requires FILE_IN) */
#define MIO_F_CLI_DIR_IN        0x00000002
/** Enable configuration of UDP passive socket input */
#define MIO_F_CLI_UDP_IN        0x00000004
/** Enable configuration of single-thread TCP passive socket input */
#define MIO_F_CLI_TCP_IN        0x00000008
/** Enable configuration of libpcap dump file and live capture input */
#define MIO_F_CLI_PCAP_IN       0x00000040
/** Default to standard input with no input specifier (requires FILE_IN) */
#define MIO_F_CLI_DEF_STDIN     0x00000080
/** Mask covering output flag bits; used internally. */
#define MIO_F_CLI_OUTMASK       0x00007F00
/** Enable configuration of file output */
#define MIO_F_CLI_FILE_OUT      0x00000100
/** Enable configuration of file output to directory */
#define MIO_F_CLI_DIR_OUT       0x00000200
/** Enable configuration of UDP active socket output */
#define MIO_F_CLI_UDP_OUT       0x00000400
/** Enable configuration of TCP active socket output */
#define MIO_F_CLI_TCP_OUT       0x00000800
/** Default to standard output with no output specifier with standard input. */
#define MIO_F_CLI_DEF_STDOUT    0x00008000

/** Input specifier (--in argument). Global; do not modify. */
extern char         *mio_ov_in;
/** Output specifier (--out argument). Global; do not modify. */
extern char         *mio_ov_out;
/** 
 * Next directory for file source (--nextdir argument),
 * empty for delete, NULL for no routing. Global; do not modify. 
 */
extern char         *mio_ov_nextdir;
/** 
 * Fail directory for file source (--faildir argument),
 * empty for delete, NULL for no routing. Global; do not modify. 
 */
extern char         *mio_ov_faildir;
/**
 * Polling delay (--poll argument). Amount of time in seconds 
 * mio_dispatch_loop() will sleep when no input is available for file source.
 * Global; do not modify.
 */
extern int          mio_ov_poll;
/** Lock option flag (TRUE if --lock present). Global; do not modify. */
extern gboolean     mio_ov_lock;
/** Live capture option flag (TRUE if --live present). Global; do not modify. */
extern gboolean     mio_ov_live;
/** BPF expression for pcap filter (--bpf argument). Global; do not modify. */
extern char         *mio_ov_bpf;

/**
 * Live capture length in octets. Global application option; set before 
 * calling mio_config_source() or mio_dispatch().
 */
extern uint32_t     mio_ov_pcaplen;
/**
 * Live capture timeout in milliseconds. Global application option; set before 
 * calling mio_config_source() or mio_dispatch().
 */
extern uint32_t     mio_ov_pcapto;
/**
 * UDP/TCP source and sink default application service; string naming a 
 * service to be passed to getaddrinfo(3)/getservbyname(3), or a string 
 * containing an integer port number. Global application option; set before 
 * calling mio_config_source(), mio_config_sink(), or mio_dispatch().
 */
extern char         *mio_ov_port;
/**
 * MIOType of the FILE_IN sources and FILE_OUT sinks; valid values are 
 * MIO_T_NULL, MIO_T_FD, and MIO_T_FP. Default is MIO_T_FP. Global 
 * application option; set before calling mio_config_source(),
 * mio_config_sink(), or mio_dispatch().
 */
extern MIOType      mio_ov_filetype;

/**
 * Add an option group appropriate for parsing MIO options consistent 
 * with the given CLI flags to the given options context.
 * 
 * @param aoctx airframe option context
 * @param flags MIO_F_CLI_* flags describing application I/O capabilities.
 * @return TRUE if successful, FALSE otherwise
 */
gboolean mio_add_option_group(AirOptionCtx *aoctx, uint32_t flags);

/**
 * Configure an MIOSource from mio_config command-line and global
 * application options. Call this after calling mio_option_group()
 * and g_option_context_parse() on a GOptionContext containing the returned
 * MIO GOptionGroup.
 * 
 * @param source    Pointer to MIOSource to configure. This MIOSource will be
 *                  overwritten.
 * @param cli_flags MIO_F_CLI_* flags describing application I/O capabilities.
 *                  Must be identical to or a subset of the flags argument
 *                  to mio_option_group(); applications may decide based on
 *                  other command-line options that certain source types are
 *                  no longer acceptable, for example.
 * @param miod_flags Pointer to an mio_dispatch() flags word. 
 *                  mio_config_source() may set or unset any MIO_F_OPT_* 
 *                  flags in this word as appropriate. 
 * @param err       An error description pointer; will contain error if
 *                  mio_config_source() was unable to configure a source.
 * @return TRUE if the MIOSource was configured successfully.
 */

gboolean mio_config_source(
    MIOSource       *source,
    uint32_t        cli_flags,
    uint32_t        *miod_flags,
    GError          **err);

/**
 * Configure an MIOSink from mio_config command-line and global
 * application options. Call this after calling mio_option_group() and 
 * g_option_context_parse() on a GOptionContext containing the returned
 * MIO GOptionGroup.
 * 
 * @param source    Pointer to a configured MIOSource (generally from 
 *                  mio_config_source()); used for determining whether to 
 *                  default to standard output (pipe-filter mode). May pass 
 *                  NULL if your application is sourceless.
 * @param sink      Pointer to MIOSink to configure. This MIOSink will be
 *                  overwritten.
 * @param basepat   Base pattern describing output filenames for FILE_OUT or
 *                  DIR_OUT modes. See mio_sink_init_file_pattern() for 
 *                  pattern substitution rules; user-specified directory
 *                  may be prepended to this pattern if supplied.
 * @param cli_flags MIO_F_CLI_* flags describing application I/O capabilities.
 *                  Must be identical to or a subset of the flags argument
 *                  to mio_option_group(); applications may decide based on
 *                  other command-line options that certain source types are
 *                  no longer acceptable, for example.
 * @param miod_flags Pointer to an mio_dispatch() flags word. 
 *                  mio_config_sink() may set or unset any MIO_F_OPT_* 
 *                  flags in this word as appropriate. 
 * @param err       An error description pointer; will contain error if
 *                  mio_config_sink() was unable to configure a sink.
 * @return TRUE if the MIOSink was configured successfully.
 */

gboolean mio_config_sink(
    MIOSource       *source,
    MIOSink         *sink,
    char            *basepat,
    uint32_t        cli_flags,
    uint32_t        *miod_flags,
    GError          **err);

/**
 * Configure a multiple MIOSink array of file sinks of from mio_config
 * command-line and global application options, as well as a set of 
 * application-defined labels. See mio_sink_multi.h and mio_sink_file.h for 
 * more. Call this after calling mio_option_group() 
 * and g_option_context_parse() on a GOptionContext containing the returned
 * MIO GOptionGroup.
 * 
 * @param source    Pointer to a configured MIOSource (generally from 
 *                  mio_config_source()); used for determining whether to 
 *                  default to standard output (pipe-filter mode). May pass 
 *                  NULL if your application is sourceless.
 * @param sink      Pointer to MIOSink to configure. This MIOSink will be
 *                  overwritten.
 * @param basepat   Base pattern describing output filenames. See 
 *                  mio_sink_init_file_pattern() for pattern substitution
 *                  rules; user-specified directory and application-specified 
 *                  labels will be prepended to this pattern.
 * @param count     Number of file sinks to create in the multiple sink; also 
 *                  defines the size of the labels array.
 * @param labels    Array of labels to attach to each output file in the 
 *                  multiple file sink.
 * @param cli_flags MIO_F_CLI_* flags describing application I/O capabilities.
 *                  Must be identical to or a subset of the flags argument
 *                  to mio_option_group(); applications may decide based on
 *                  other command-line options that certain source types are
 *                  no longer acceptable, for example.
 * @param miod_flags Pointer to an mio_dispatch() flags word. 
 *                  mio_config_multisink_file() may set or unset any 
 *                  MIO_F_OPT_* flags in this word as appropriate. 
 * @param err       An error description pointer; will contain error if
 *                  mio_config_sink() was unable to configure a sink.
 * @return TRUE if the MIOSink was configured successfully.
 */

gboolean mio_config_multisink_file(
    MIOSource       *source,
    MIOSink         *sink,
    char            *basepat,
    uint32_t        count,
    char            **labels,
    uint32_t        cli_flags,
    uint32_t        *miod_flags,
    GError          **err);

/* end idem */
#endif
