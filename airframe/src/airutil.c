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

#define _AIRFRAME_SOURCE_
#include <airframe/airutil.h>

static char *RCSID __attribute__ ((unused)) = 
    "$Id: airutil.c 17466 2011-07-20 20:15:43Z ecoff_svn $";

#define AIR_TIME_ISO8601_FMT     "%04u-%02u-%02u %02u:%02u:%02u"
#define AIR_TIME_ISO8601_NS_FMT  "%04u-%02u-%02uT%02u:%02u:%02u"
#define AIR_TIME_ISO8601_HMS_FMT "%02u:%02u:%02u"
#define AIR_TIME_SQUISHED_FMT    "%04u%02u%02u%02u%02u%02u"


static const char *air_time_select_format(
    AirTimeFormat   fmtid)
{
    switch (fmtid) {
        case AIR_TIME_ISO8601:
            return AIR_TIME_ISO8601_FMT;
        case AIR_TIME_ISO8601_NS:
            return AIR_TIME_ISO8601_NS_FMT;
        case AIR_TIME_SQUISHED:
            return AIR_TIME_SQUISHED_FMT;
        case AIR_TIME_ISO8601_HMS:
            return AIR_TIME_ISO8601_HMS_FMT;
    }
    return AIR_TIME_ISO8601_FMT;
}

void air_time_g_string_append(
    GString         *str,
    time_t          time,
    AirTimeFormat   fmtid)
{
    struct tm       time_tm;
    const char      *fmt;
    
    fmt = air_time_select_format(fmtid);
    
    gmtime_r(&time, &time_tm);
    if (AIR_TIME_ISO8601_HMS == fmtid) {
	g_string_append_printf(str, fmt, 
			       time_tm.tm_hour,
			       time_tm.tm_min,
			       time_tm.tm_sec);
    } else {
	g_string_append_printf(str, fmt,
			       time_tm.tm_year + 1900,
			       time_tm.tm_mon + 1,
			       time_tm.tm_mday,
			       time_tm.tm_hour,
			       time_tm.tm_min,
			       time_tm.tm_sec);
    }

}

void air_time_buf_print(
    char           *buf,
    time_t          time,
    AirTimeFormat   fmtid)
{
    struct tm       time_tm;
    const char      *fmt;
    
    fmt = air_time_select_format(fmtid);
    
    gmtime_r(&time, &time_tm);
    snprintf(buf, AIR_TIME_BUF_MINSZ, fmt,
            time_tm.tm_year + 1900,
            time_tm.tm_mon + 1,
            time_tm.tm_mday,
            time_tm.tm_hour,
            time_tm.tm_min,
            time_tm.tm_sec);
}

void air_mstime_g_string_append(
    GString         *str,
    uint64_t        mstime,
    AirTimeFormat   fmtid)
{
    uint32_t        msrem;
    
    msrem = mstime % 1000;
    mstime = mstime / 1000;
    
    air_time_g_string_append(str, mstime, fmtid);
    g_string_append_printf(str, ".%03u", msrem);
}

/* Note: heavily inspired by Python 2.2 timegm() implementation. */

time_t air_time_gm(
    uint32_t        year,
    uint32_t        mon,
    uint32_t        day,
    uint32_t        hour,
    uint32_t        min,
    uint32_t        sec)
{
    static uint32_t dpm[] = { 31, 28, 31, 30,
                              31, 30, 31, 31,
                              30, 31, 30, 31 };
    time_t          epoday = 0, eposec = 0;
    uint32_t        i;
    
    /* assert input validity */
    g_assert(year >= 1970);
    g_assert((mon >= 1) && (mon <= 12));
    
    /* calculate day offset to beginning of year */
    epoday = (365 * (year - 1970)) +
                (((year - 1)/4 - 1969/4) -
                 ((year - 1)/100 - 1969/100) +
                 ((year - 1)/400 - 1969/400));
    
    /* add day offset to beginning of month */
    for (i = 1; i < mon; i++) {
        epoday += dpm[i-1];
    }
    
    /* add day offset for leap year */
    if (mon > 2 &&
        ((!(year % 4) && (year % 100)) || !(year % 400))) {
        epoday += 1;
    }
    
    /* add day offset for beginning of day */
    epoday += (day - 1);
    
    /* convert day offset to epoch */
    eposec = epoday * 86400;
    
    /* add hour, minute, second offset */
    eposec += hour * 3600;
    eposec += min * 60;
    eposec += sec;
    
    /* All done. */
    return eposec;
}

void air_ipaddr_buf_print(
    char            *buf,
    uint32_t         ipaddr)
{
    uint8_t          dqp[4];
    uint32_t         mask = 0xff000000U;

    /* split the address */
    dqp[0] = (ipaddr & mask) >> 24;
    mask >>= 8;
    dqp[1] = (ipaddr & mask) >> 16;
    mask >>= 8;
    dqp[2] = (ipaddr & mask) >> 8;
    mask >>= 8;
    dqp[3] = (ipaddr & mask);

    /* print to it */
    snprintf(buf,AIR_IPADDR_BUF_MINSZ,
             "%hhu.%hhu.%hhu.%hhu",dqp[0],dqp[1],dqp[2],dqp[3]);
}

void air_ip6addr_buf_print(
    char            *buf,
    uint8_t         *ipaddr)
{
    char            *cp = buf;
    uint16_t        *aqp = (uint16_t *)ipaddr;
    uint16_t        aq;
    gboolean        colon_start = FALSE;
    gboolean        colon_end = FALSE;
    
    for (; (uint8_t *)aqp < ipaddr + 16; aqp++) {
        aq = g_ntohs(*aqp);
        if (aq || colon_end) {
            if ((uint8_t *)aqp < ipaddr + 14) {
                snprintf(cp, 6, "%04hx:", aq);
                cp += 5;
            } else {
                snprintf(cp, 5, "%04hx", aq);
                cp += 4;    
            }
            if (colon_start) {
                colon_end = TRUE;
            }
        } else if (!colon_start) {
            if ((uint8_t *)aqp == ipaddr) {
                snprintf(cp, 3, "::");
                cp += 2;
            } else {
                snprintf(cp, 2, ":");
                cp += 1;
            }
            colon_start = TRUE;
        }
    }
}

uint32_t air_mask_from_prefix(
    uint32_t            pfx) {

    uint32_t            mask = 0;
    uint32_t            i = 0;
    
    for (i = 0; i < pfx; i++) {
        mask >>= 1;
        mask |= 0x80000000U;
    }
    
    return mask;
}

static uint32_t air_hexdump_g_string_append_line(
    GString             *str,
    char                *lpfx,
    uint8_t             *cp,
    uint32_t            lineoff,
    uint32_t            buflen)
{
    uint32_t            cwr = 0, twr = 0;
    
    /* stubbornly refuse to print nothing */
    if (!buflen) return 0;
    
    /* print line header */
    g_string_append_printf(str, "%s %04x:", lpfx, lineoff);
    
    /* print hex characters */
    for (twr = 0; twr < 16; twr++) {
        if (buflen) {
            g_string_append_printf(str, " %02hhx", cp[twr]);
            cwr++; buflen--;
        } else {
            g_string_append(str, "   ");
        }
    }
    
    /* print characters */
    g_string_append_c(str, ' ');
    for (twr = 0; twr < cwr; twr++) {
        if ((cp[twr] > 32 && cp[twr] < 128) || cp[twr] == 32) {
            g_string_append_c(str, cp[twr]);
        } else {
            g_string_append_c(str, '.');
        }
    }
    g_string_append_c(str, '\n');
    
    return cwr;
}

void air_hexdump_g_string_append(
    GString             *str,
    char                *lpfx,
    uint8_t             *buf,
    uint32_t            len)
{
    uint32_t            cwr = 0, lineoff = 0;
    
    do {
        cwr = air_hexdump_g_string_append_line(str, lpfx, buf, lineoff, len);
        buf += cwr; len -= cwr; lineoff += cwr;
    } while (cwr == 16);
}

gboolean air_sock_maxrcvbuf(
    int         sock,
    int         *size)
{
    while (*size > 4096) {
        if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, size, sizeof(*size)) == 0)
            return TRUE;
        if (errno != ENOBUFS) return FALSE;
        *size -= (*size > 1024 * 1024) 
                        ? 1024 * 1024 
                        : 2048;
    }
    return FALSE;
}

gboolean air_sock_maxsndbuf(
    int         sock,
    int         *size)
{
    while (*size > 4096) {
        if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, size, sizeof(*size)) == 0)
            return TRUE;
        if (errno != ENOBUFS) return FALSE;
        *size -= (*size > 1024 * 1024) 
                        ? 1024 * 1024 
                        : 2048;
    }
    return FALSE;
}

void air_ignore_sigpipe()
{
    struct sigaction sa, osa;

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGPIPE,&sa,&osa)) {
        g_error("sigaction(SIGPIPE) failed: %s", strerror(errno));
    }
}
