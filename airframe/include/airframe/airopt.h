/*
** airopt.h
** Airframe options interface
**
** ------------------------------------------------------------------------
** Copyright (C) 2007-2011 Carnegie Mellon University. All Rights Reserved.
** ------------------------------------------------------------------------
** Authors: Tony Cebzanov <tonyc@cert.org>
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
 * Airframe options interface.
 */

/* idem hack */
#ifndef _AIR_AIROPT_H_
#define _AIR_AIROPT_H_

#include <airframe/autoinc.h>

#if USE_GOPTION

typedef GOptionEntry AirOptionEntry;

/** Macro used to define command-line options
 *
 * @param longname The full name of the option
 * @param shortname A single character identifier for the option
 * @param flag Special option flags.  Currently unused.
 * @param type The data type (one of AF_OPT_TYPE_*) for the option's value.
 * @param var Pointer to the location where the option value will be stored.
 * @param desc Description of the option in help output.
 * @param vardesc Description of the option's value in help output.
 */
#define AF_OPTION(longname, shortname, flag, type, var, desc, vardesc) \
    { longname, shortname, flag, type, var, desc, vardesc }

/**
 * Macro used to terminate an AF_OPTION list
 */
#define AF_OPTION_END { NULL }

/**
 * Macro to test if an AF_OPTION structure is empty
 */
#define AF_OPTION_EMPTY(option) ( option.long_name == NULL )

/** No option argument */
#define AF_OPT_TYPE_NONE   G_OPTION_ARG_NONE

/** Integer option argument */
#define AF_OPT_TYPE_INT    G_OPTION_ARG_INT

/** String option argument */
#define AF_OPT_TYPE_STRING G_OPTION_ARG_STRING

/** Double-precision argument */
#define AF_OPT_TYPE_DOUBLE G_OPTION_ARG_DOUBLE

#elif USE_POPT

#include <popt.h>

typedef struct poptOption AirOptionEntry;

/** Macro used to define command-line options
 *
 * @param longname The full name of the option
 * @param shortname A single character identifier for the option
 * @param flag Special option flags.  Currently unused.
 * @param type The data type (one of AF_OPT_TYPE_*) for the option's value.
 * @param var Pointer to the location where the option value will be stored.
 * @param desc Description of the option in help output.
 * @param vardesc Description of the option's value in help output.
 */
#define AF_OPTION(longname, shortname, flag, type, var, desc, vardesc) \
    { longname, shortname, type, var, flag, desc, vardesc }

/**
 * Macro used to terminate an AF_OPTION list
 */
#define AF_OPTION_END POPT_TABLEEND

/**
 * Macro to test if an AF_OPTION structure is empty
 */
#define AF_OPTION_EMPTY(option)   \
    ( option.longName == NULL     \
      && option.shortName == '\0' \
      && option.argInfo == 0 )

/** No option argument */
#define AF_OPT_TYPE_NONE   POPT_ARG_NONE

/** Integer option argument */
#define AF_OPT_TYPE_INT    POPT_ARG_INT

/** String option argument */
#define AF_OPT_TYPE_STRING POPT_ARG_STRING

/** Double-precision option argument */
#define AF_OPT_TYPE_DOUBLE POPT_ARG_DOUBLE

#else

#error A suitable GLib or popt library was not found for options processing.

#endif

/**
 * Opaque options context structure.
 */
typedef struct _AirOptionCtx AirOptionCtx;

/**
 * Print a formatted option error message on standard error and exit the 
 * process. Use this only during command-line option processing. This call 
 * will not return.
 * 
 * @param fmt format string of error message
 */
 
void air_opterr(
    const char      *fmt,
    ...);

/**
 * Create a new option context.
 *
 * @param helpstr Text to be displayed after the name of the command in help
 * @param argc The address of the program's argc count
 * @param argv The address of the program's argv array
 * @param entries An array of AF_OPTION structures terminated by AF_OPTION_END
 * @return An initialized AirOptionCtx, or NULL if an error occurred.
 */
AirOptionCtx *air_option_context_new(
    const char     *helpstr,
    int            *argc,
    char         ***argv,
    AirOptionEntry *entries);

/**
 * Add a group of options to an option context.
 *
 * @param aoctx AirOptionCtx to be modified
 * @param shortname A short name for the group, which should not contains spaces
 * @param longname The full name of the option group, shown in help
 * @param description A brief description of the option group shown in help
 * @param entries An array of AF_OPTION structures terminated by AF_OPTION_END
 * @return TRUE if group add was successful, FALSE otherwise
 */
gboolean air_option_context_add_group(
    AirOptionCtx   *aoctx,
    const char     *shortname,
    const char     *longname,
    const char     *description,
    AirOptionEntry *entries);

/**
 * Parse command line arguments based on option entries that have been added
 * to the option context.  The argc and argv associated with the context will
 * be updated by this function, with recognized options removed. Prints
 * an error to standard error and terminates the process if the command-line
 * cannot be parsed.
 *
 * @param aoctx AirOptionCtx to be parsed
 */
void air_option_context_parse(
    AirOptionCtx *aoctx);

/**
 * Enable the display of option help by invoking your program with the --help
 * or --usage parameters.
 *
 * @param aoctx AirOptionCtx to be modified.
 */
void air_option_context_set_help_enabled(
    AirOptionCtx *aoctx);

/**
 * Print a command line option usage message for your program, if supported by
 * the underlying options library.
 *
 * @param aoctx AirOptionCtx to be displayed.
 */
void air_option_context_usage(
    AirOptionCtx *aoctx);

/**
 * Destroy an options context.
 *
 * @param aoctx AirOptionCtx to be freed.
 */
void air_option_context_free(
    AirOptionCtx *aoctx);



/* end idem */
#endif

