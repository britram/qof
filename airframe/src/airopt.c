/*
** airopt.c
** Airframe options interface
**
** ------------------------------------------------------------------------
** Copyright (C) 2005-2011 Carnegie Mellon University. All Rights Reserved.
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

#define _AIRFRAME_SOURCE_
#include <airframe/airopt.h>

static char *RCSID __attribute__(
    (unused)) =
    "$Id: airutil.c 4728 2006-08-30 14:41:01Z bht $";

struct _AirOptionCtx {
#if USE_GOPTION
    GOptionContext *octx;
#elif USE_POPT
    poptContext     octx;
    GArray         *options;
#endif
    int            *argc;
    char         ***argv;
};

void air_opterr(
    const char      *fmt,
    ...)
{
    va_list         ap;

    fprintf(stderr, "Command-line argument error: \n");

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\nUse --help for usage.\n");

    exit(1);
}

AirOptionCtx *air_option_context_new(
    const char     *helpstr,
    int            *argc,
    char         ***argv,
    AirOptionEntry *entries)
{
    AirOptionCtx *aoctx;
#if USE_GOPTION
    GOptionContext  *octx = NULL;
#elif USE_POPT
    poptContext octx = NULL;
    int i    = 0;
#endif

    aoctx = g_new0(AirOptionCtx, 1);
#if USE_GOPTION
    octx = g_option_context_new(helpstr);
    if (entries) {
        g_option_context_add_main_entries(octx, entries, NULL);
    }
#elif USE_POPT

    aoctx->options = g_array_sized_new(TRUE, TRUE, sizeof(AirOptionEntry), 64);
    if (entries) {

        for (i=0; !AF_OPTION_EMPTY(entries[i]); i++) {
            g_array_append_val(aoctx->options, entries[i]);
        }
    }
    octx = poptGetContext(NULL, *argc,  (const char **) *argv,
                          (AirOptionEntry *) aoctx->options->data, 0);

    poptSetOtherOptionHelp(octx, helpstr);
#endif

    aoctx->argc = argc;
    aoctx->argv = argv;
    aoctx->octx = octx;

    return aoctx;
}


gboolean air_option_context_add_group(
    AirOptionCtx   *aoctx,
    const char     *shortname,
    const char     *longname,
    const char     *description,
    AirOptionEntry *entries)
{
    g_assert(aoctx != NULL);
    g_assert(aoctx->octx != NULL);

#if USE_GOPTION
    {
        GOptionGroup *ogroup;

        /* create an option group */
        ogroup = g_option_group_new(shortname, longname,
                                    description, NULL, NULL);
        g_option_group_add_entries(ogroup, entries);
        g_option_context_add_group(aoctx->octx, ogroup);

        return TRUE;
    }
#elif USE_POPT
    {
        struct poptOption poption;

        poption.longName   = NULL;
        poption.shortName  = '\0';
        poption.argInfo    = POPT_ARG_INCLUDE_TABLE;
        poption.arg        = entries;
        poption.val        = 0;
        poption.descrip    = longname;
        poption.argDescrip = NULL;
        g_array_append_val(aoctx->options, poption);

        return TRUE;
    }
#endif

    return FALSE;
}

void air_option_context_parse(
    AirOptionCtx *aoctx)
{
#if USE_GOPTION
    GError          *oerr = NULL;

    g_option_context_parse(aoctx->octx, aoctx->argc, aoctx->argv, &oerr);
    if (oerr) {
        air_opterr("%s", oerr->message);
    }
#elif USE_POPT
    {
        int argcount = 0;
        char **rest     = 0;
        int rc;

        GPtrArray *new_argv = NULL;

        rc = poptGetNextOpt(aoctx->octx);
        if (rc != -1) {
            air_opterr("%s", poptStrerror(rc));
        }

        /* We have to manually construct the argv here because GLib keeps the
         * program name in argv[0] and popt doesn't. */
        new_argv = g_ptr_array_sized_new(64);
        g_ptr_array_add(new_argv, g_strdup(*(aoctx->argv)[0]));

        /* Do the actual parsing, returning non-switch args */
        rest = (char **) poptGetArgs(aoctx->octx);

        /* Walk through the remaining args, adding them to the new argv and
         * counting them for argc */
        while ( (rest != NULL) && rest[argcount] != NULL) {
            g_ptr_array_add(new_argv, g_strdup(rest[argcount]));
            argcount++;
        }
        g_ptr_array_add(new_argv, NULL);
        /* Now replace the original argc and argv with post-parse values */
        *(aoctx->argc) = argcount;
        *(aoctx->argv) = (char **) g_ptr_array_free(new_argv, FALSE);
    }
#endif
}

void air_option_context_set_help_enabled(
    AirOptionCtx *aoctx)
{
    g_assert(aoctx != NULL);
    g_assert(aoctx->octx != NULL);
#if USE_GOPTION
    g_option_context_set_help_enabled(aoctx->octx, TRUE);
#elif USE_POPT
    {
        struct poptOption poption;

        poption.longName   = NULL;
        poption.shortName  = '\0';
        poption.argInfo    = POPT_ARG_INCLUDE_TABLE;
        poption.arg        = poptHelpOptions;
        poption.val        = 0;
        poption.descrip    =  "Help options:";
        poption.argDescrip = NULL;
        g_array_append_val(aoctx->options,  poption);
    }
#endif
}

void air_option_context_usage(
    AirOptionCtx *aoctx)
{
    g_assert(aoctx != NULL);
    g_assert(aoctx->octx != NULL);

#if USE_GOPTION
    /* Grr.  GLib has a g_option_context_get_help() function, but it's new as
     * of GLib 2.14, which almost nobody has.  So, we timidly tell the user
     * to get the usage for themselves instead of printing it for them.
     * GLib--
     *
     * g_fprintf(stderr, g_option_context_get_help(aoctx->octx, FALSE, NULL));
     */

    g_fprintf(stderr, "Use --help for usage.");
#elif USE_POPT
    poptPrintHelp(aoctx->octx, stderr, 0);
#endif
    return;
}

void air_option_context_free(
    AirOptionCtx *aoctx)
{
#if USE_GOPTION
    g_option_context_free(aoctx->octx);
    g_free(aoctx);
#elif USE_POPT
    g_array_free(aoctx->options, TRUE);
    poptFreeContext(aoctx->octx);
#endif

}
