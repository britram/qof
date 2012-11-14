/*
 ** airutil.c
 ** General utility functions
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

/* Yeah, this is a dumb place for mainpage */

/** 
 * @mainpage Airframe Application Utility Library
 *
 * @section Introduction
 *
 * Airframe is an application utility library designed to ease the creation of
 * command-line applications written in C that process data from a variety of
 * input sources to a variety of output sources. It builds atop the fundamental
 * data structures and utilities in glib (http://www.gtk.org) 2.0, adding
 * additional higher-level functionality.
 *
 * All of the Airframe modules provide their own command-line processing atop 
 * the GOption facility; in this way, all Airframe applications have similar 
 * command-line user interfaces.
 *
 * Airframe was originally developed for NAF 
 * (http://tools.netsa.cert.org/naf), and is the mechanism by which the NAF 
 * tools have a common interface. It evolved from the libair common library 
 * used by the AirCERT project (http://aircert.sourceforge.net). It is released 
 * as a separate library in the hopes that other applications developers may 
 * find it useful.
 *
 * @section Modules
 *
 * Airframe provides four modules which may be used to ease application
 * creation. The daeconfig module (defined in daeconfig.h) handles 
 * user-controlled daemonization, compatible with the filedaemon pattern. 
 * The privconfig module (defined in privconfig.h) handles
 * user-controlled privilege management. The logconfig module (defined in 
 * logconfig.h) handles user-controlled log message routing. The mio module 
 * (defined in mio.h and mio_config.h) handles user-controlled file, network, 
 * and packet capture I/O and record-oriented dispatch. 
 *
 * @section Utilities
 * 
 * Additional utility functions we've found useful in building applications 
 * for handling network event data are defined in airutil.h. airopt.h defines 
 * a command-line options processing layer atop glib-2 or popt, and is used 
 * by Airframe's modules and client applications. Also, airlock.h allows 
 * applications not using MIO for I/O and dispatch to interoperate with 
 * filedaemon-style locking.
 *
 * The filedaemon pattern implemented by MIO may be wrapped around any 
 * stdin-to-stdout filter application using the
 * <a href="../filedaemon.1.pdf">filedaemon</a> application built and installed
 * with Airframe.
 * 
 * Airframe also includes an application called
 * <a href="../airdaemon.1.pdf">airdaemon</a> which simply invokes
 * a child process and restarts it after a configurable delay if it exits
 * abnormally.  airdaemon can retry after a fixed delay, or use a binary
 * exponential backoff strategy to increase the retry delay.
 *
 * @section Copyright
 *
 * Airframe is copyright 2006-2008 Carnegie Mellon University, and is released 
 * under the GNU Lesser General Public License. See the COPYING file in the 
 * distribution for details.
 *
 * Airframe was developed at the CERT Network Situational Awareness Group
 * by Brian Trammell <bht@cert.org> for use in the NAF tools; certain modules
 * were written by Tony Cebzanov <tonyc@cert.org>
 */
 
/**
 * @file
 * 
 * Airframe utility functions. A home for utility functions that have no other.
 */

/* idem hack */
#ifndef _AIR_AIRUTIL_H_
#define _AIR_AIRUTIL_H_

#include <airframe/autoinc.h>

/** Minimum buffer size for air_time_buf_print() */
#define AIR_TIME_BUF_MINSZ       20
/** Minimum buffer size for air_ipaddr_buf_print() */
#define AIR_IPADDR_BUF_MINSZ     16
/** Minimum buffer size for air_ip6addr_buf_print() */
#define AIR_IP6ADDR_BUF_MINSZ    40

/** Time format description for air_time_* functions */
typedef enum air_timefmt_en {
    /** ISO8601 format with space between date and time */
    AIR_TIME_ISO8601,
    /** ISO8601 format with T between date and time */
    AIR_TIME_ISO8601_NS,
    /** Time format squished into single string for ASCII sort by time. */
    AIR_TIME_SQUISHED,
    /** ISO8601 format with time only */
    AIR_TIME_ISO8601_HMS
} AirTimeFormat;

/**
 * Append a string format of a time to a given GString in a given format.
 *
 * @param str string to append to
 * @param time time to append in epoch seconds
 * @param fmtid time format description
 */

void air_time_g_string_append(
    GString         *str,
    time_t          time,
    AirTimeFormat   fmtid);

/**
 * Append a string format of a time in epoch milliseconds to a given GString 
 * in a given format.
 *
 * @param str string to append to
 * @param mstime time to append in epoch milliseconds
 * @param fmtid time format description
 */
 
void air_mstime_g_string_append(
    GString         *str,
    uint64_t        mstime,
    AirTimeFormat   fmtid);

/**
 * Write a string format of a time to a given buffer in a given format.
 *
 * @param buf buffer to write to
 * @param time time to print in epoch seconds
 * @param fmtid time format description
 */
        
void air_time_buf_print(
    char           *buf,
    time_t          time,
    AirTimeFormat   fmtid);

/**
 * Portable, less unix-ish timegm() implementation. Converts a UTC year, 
 * month, day, hour, minute, and second into a time in 
 * epoch seconds. Handles leap years but not leap seconds. mon is 1-based
 * (as in English representations), and year is CE, not 1900-based.
 *
 * @param year year of date to convert (year CE)
 * @param mon month of date to convert (1-12)
 * @param day day of date to convert (1-31)
 * @param hour hour of date to convert (0-23)
 * @param min minute of date to convert (0-59)
 * @param sec second of date to convert (0-59)
 * @return epoch seconds
 */
 
time_t air_time_gm(
    uint32_t        year,
    uint32_t        mon,
    uint32_t        day,
    uint32_t        hour,
    uint32_t        min,
    uint32_t        sec);

/**
 * Write the dotted quad format of an IPv4 address to a given buffer.
 * The buffer must be at least AIR_IPADDR_BUF_MINSZ (16) bytes long.
 *
 * @param buf buffer to write to
 * @param ipaddr address to print
 */

void air_ipaddr_buf_print(
    char            *buf,
    uint32_t         ipaddr);

/**
 * Write the presentation format of an IPv6 address to a given buffer.
 * The buffer must be at least AIR_IP6ADDR_BUF_MINSZ (40) bytes long.
 *
 * @param buf buffer to write to
 * @param ipaddr address to print
 */
void air_ip6addr_buf_print(
    char            *buf,
    uint8_t         *ipaddr);

/**
 * Given a CIDR prefix length, return a mask for extracting the network part
 * of the address.
 *
 * @param pfx prefix length (0-32)
 * @return network-part mask bits
 */
 
uint32_t air_mask_from_prefix(
    uint32_t            pfx);

/**
 * Append a given binary buffer as a hex + ASCII dump with 16 bytes per line
 * to the given GString.
 *
 * @param str       string to append to
 * @param lpfx      string to prefix each line of output with. 
 *                  Use for indentation and labeling.
 * @param buf       Buffer to dump to GString
 * @param len       Length of buf
 */
 
void air_hexdump_g_string_append(
    GString             *str,
    char                *lpfx,
    uint8_t             *buf,
    uint32_t            len);

/**
 * Maximize socket receive buffer size. Sets the socket's receive buffer to 
 * the highest available size less than or equal to the given size.
 *
 * @param sock socket to increase buffer size of
 * @param size pointer to maximum size to set. Returns actually set size.
 * @return TRUE on success, FALSE otherwise.
 */

gboolean air_sock_maxrcvbuf(
    int         sock,
    int         *size);

/**
 * Maximize socket send buffer size. Sets the socket's receive buffer to 
 * the highest available size less than or equal to the given size.
 *
 * @param sock socket to increase buffer size of
 * @param size pointer to maximum size to set. Returns actually set size.
 * @return TRUE on success, FALSE otherwise.
 */

gboolean air_sock_maxsndbuf(
    int         sock,
    int         *size);

/**
 * Ignore SIGPIPE, so that failed pipe writes or TCP writes on reset sockets 
 * will return EPIPE instead of terminating the application.
 */
 
void air_ignore_sigpipe();
    
/* end idem */
#endif
