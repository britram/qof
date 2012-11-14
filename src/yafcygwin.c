/**
 ** @internal
 ** yafcygwin.c
 ** YAF cygwin
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2011-2012 Carnegie Mellon University. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Chris Inacio
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

/* Microsoft says to define _WIN32_WINNT to get the right
   windows API version, but under Cygwin, you need to define
   WINVER - which are related, (but not the same?).  They
   are believed to be the same under Cygwin */

#ifdef __CYGWIN__
#define _WIN32_WINNT 0x0600
#define WINVER 0x0600
#include <windows.h>
#endif

#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <stdio.h>


#define INITIAL_BUFFER_SIZE     8192
#define BUFFER_INCREMENT_SIZE   4096

/* for testing
#define NETSA_WINDOWSREG_REGHOME        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"
#define SILK_WINDOWSREG_DATA_DIR_KEY    "SystemRoot"
*/

/* registry location/key definitions */
#define NETSA_WINDOWSREG_REGHOME        "Software\\CERT\\NetSATools"
#define NETSA_WINDOWSREG_INSTALLDIR_KEY         "InstallDir"
#define SILK_WINDOWSREG_DATA_DIR_KEY            "SilkDataDir"
#define YAF_WINDOWSREG_CONF_DIR_KEY             "YafConfDir"

#define SILK_DEFAULT_CYGWIN_DATA_DIR            "/cygdrive/c/data"

#define CYGWIN_PATH_PREFIX                      "/cygdrive/"

static char *winRegDataDir = NULL;


/**
 * yfCygwinClean
 *
 * frees up allocated memory used as caching within this cygwin module
 * provided for future memory leak testing
 *
 */
void
yfCygwinClean (void)
{

    if (NULL != winRegDataDir) {
        free(winRegDataDir);
    }

    return;
}

/**
 *windowsToCygwinPath
 *
 * converts a "normal" windows path "C:\Windows\" into an equivalent
 * cygwin path "/cygdrive/c/Windows/"
 *
 * @note this function creates callee deallocated memory
 *
 * @param winPath a character string containing a windows path
 *
 * @return a malloced string converted into a cygwin path, on error
 *         this function returns NULL
 */
static
char *
windowsToCygwinPath (const char *winPath)
{

    char *resultStr = NULL;
    char *resultLoc = NULL;
    const char *workLoc = winPath;

    resultStr = (char *) malloc(strlen(winPath)+strlen(CYGWIN_PATH_PREFIX)+1);
    if (NULL == resultStr) {
        return NULL;
    }
    resultLoc = resultStr;

    /* include the default prefix */
    strcpy(resultLoc, CYGWIN_PATH_PREFIX);
    resultLoc += strlen(CYGWIN_PATH_PREFIX);

    /* first, let's try to find the drive prefix, e.g. c: or d: or z: */
    workLoc = strchr(winPath, ':');
    if (NULL == workLoc) {
        /* it's a relative path, run with it? */
        free(resultStr);
        return NULL;
    }

    /* the character before workLoc should be the drive letter */
    strncpy(resultLoc, (workLoc-1), 1);
    *resultLoc++ = (char) tolower((int)*(workLoc-1));
    workLoc++;

    /* now copy in the rest of the path, converting "\" into "/" */
    while (*workLoc) {
        if ('\\' == *workLoc) {
            *resultLoc = '/';
        } else {
            *resultLoc = *workLoc;
        }
        resultLoc++; workLoc++;
    }

    /* make sure resultLoc is terminated */
    *resultLoc = '\0';

    /* safety check, did we run off the end of resultLoc */
    if ((resultLoc - resultStr) > (strlen(winPath)+strlen(CYGWIN_PATH_PREFIX)+1)) {
        abort();
    }

    /* return the converted string */
    return resultStr;
}



/**
 * yfGetCygwinConfDir
 *
 * Gets the yaf config directory defined at INSTALLATION time on
 * Windows machines via reading the windows registry.
 * Caches the result in a file static.
 *
 * @return constant string with the data directory name
 *
 * @note must call yfCygwinClean to get rid of the memory
 *       for the cached result
 */
const char *
yfGetCygwinConfDir ()
{

    char *dataBuffer = NULL;
    DWORD bufferSize = 0;
    DWORD rc;


    if (NULL != winRegDataDir) {
        return winRegDataDir;
    }

    /* allocate memory for the initial buffer,
       likely this is big enough */
    dataBuffer = (char *) malloc( sizeof(char) * INITIAL_BUFFER_SIZE);
    if (NULL == dataBuffer) {
        /* error couldn't allocate memory */
        return NULL;
    }
    bufferSize = INITIAL_BUFFER_SIZE;

    /* keeping asking the registry for the value until we have
       a buffer big enough to hold it */
    do {
        rc = RegGetValue ( HKEY_LOCAL_MACHINE,
                           NETSA_WINDOWSREG_REGHOME,
                           SILK_WINDOWSREG_DATA_DIR_KEY, RRF_RT_ANY,
                           NULL, (PVOID)dataBuffer, &bufferSize);

        if (ERROR_MORE_DATA == rc) {
            dataBuffer = (char *) realloc (dataBuffer,
                                           (bufferSize + BUFFER_INCREMENT_SIZE));
            if (NULL == dataBuffer) {
                return NULL;
            }
            bufferSize += BUFFER_INCREMENT_SIZE;
        }
    } while ( ERROR_MORE_DATA == rc);

    if ( ERROR_SUCCESS == rc ) {
        if ( 0 == bufferSize ) {
            /* What makes sense to do when we can't find the registry entry?
               In this case, we return a "sane" default for windows
            */
            winRegDataDir = SILK_DEFAULT_CYGWIN_DATA_DIR;
            free(dataBuffer);
            return SILK_DEFAULT_CYGWIN_DATA_DIR;
        } else {
            winRegDataDir = windowsToCygwinPath(dataBuffer);
            free(dataBuffer);
            return winRegDataDir;
        }

    } else {

        return NULL;
    }
}
