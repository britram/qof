/*
 ** mio_sink_file.c
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

#define _AIRFRAME_SOURCE_
#include <airframe/mio_sink_file.h>
#include <airframe/mio_stdio.h>
#include <airframe/airutil.h>

typedef struct _MIOSinkFileContext {
    GString         *scratch;
    char            *lpath;
    int             lfd;
} MIOSinkFileContext;

static gboolean mio_sink_open_file(
    MIOSink                 *sink,
    uint32_t                *flags,
    GError                  **err)
{
    MIOSinkFileContext      *fx = (MIOSinkFileContext *)sink->ctx;
    int                     fd;
    
    /* Attempt lock */
    if (*flags & MIO_F_OPT_LOCK) {
        /* Generate lock path */
        if (!fx->scratch) fx->scratch = g_string_new("");
        g_string_printf(fx->scratch, "%s.lock", sink->name);
        fx->lpath = g_strdup(fx->scratch->str);
        /* Open lock file */
        fx->lfd = open(fx->lpath, O_WRONLY | O_CREAT | O_EXCL, 0664);
        if (fx->lfd < 0) {
            g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_LOCK,
                        "Cannot lock output file %s: %s",
                        sink->name, strerror(errno));
            *flags |= MIO_F_CTL_ERROR;
            return FALSE;
        }
    }        
        
    /* Open the file if necessary */
    if (sink->vsp_type != MIO_T_NULL) {
        /* Not a null type sink. Open the file. */
        fd = open(sink->name, O_WRONLY | O_CREAT | O_TRUNC, 0664);
        if (fd < 0) {
            g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_LOCK,
                        "Cannot open output file %s: %s",
                        sink->name, strerror(errno));
            *flags |= MIO_F_CTL_ERROR;
            if (fx->lpath) unlink(fx->lpath);
            return FALSE;
        }
        
        /* Determine how to store opened file */
        if (sink->vsp_type == MIO_T_FP) {
            /* As file handle. fdopen should never fail here. */
            sink->vsp = fdopen(fd, "w");
            g_assert(sink->vsp);
        } else {
            /* As file descriptor. Yay Casting! */
            sink->vsp = GINT_TO_POINTER(fd);
        }
    }
    
    return TRUE;
}

static gboolean mio_sink_next_file_single(
    MIOSource               *source,
    MIOSink                 *sink,
    uint32_t                *flags,
    GError                  **err)
{
    /* Name and spec are identical */
    sink->name = g_strdup(sink->spec);
    
    /* Open the file */
    return mio_sink_open_file(sink, flags, err);
}

static void mio_sink_file_pattern_decname(
    char                    *srcname,
    char                    **decname,
    char                    **dirname,
    char                    **basename,
    char                    **extname)
{
    if (srcname) {
        *decname = g_strdup(srcname);
    
        if ((*extname = strrchr(*decname, '.'))) {
            **extname = (char)0;
            (*extname)++;
        } else {
            *extname = NULL;
        }
    
        if ((*basename = strrchr(*decname, '/'))) {
            **basename = (char)0;
            (*basename)++;
            *dirname = *decname;
        } else {
            *dirname = NULL;
            *basename = *decname;
        }
    } else {
        *decname = g_strdup(".");
        *dirname = *decname;
        *basename = *decname + 1;
        *extname = NULL;
    }
}

static void mio_sink_file_pattern_to_name(
    MIOSource               *source,
    MIOSink                 *sink)
{
    MIOSinkFileContext      *fx = (MIOSinkFileContext *)sink->ctx;
    MIOSinkFileConfig       *cfg = (MIOSinkFileConfig *)sink->cfg;
    char                    *cp = NULL, *decname = NULL, 
                            *dirname = NULL, *basename = NULL, *extname= NULL;
    
    /* ensure we have an empty scratch string */
    if (fx->scratch) {
        g_string_truncate(fx->scratch, 0);
    } else {
        fx->scratch = g_string_new("");
    }
    
    /* iterate over characters in the sink specifier */
    for (cp = sink->spec; *cp; cp++) {
        if (*cp == '%') {
            /* Percent character. Determine what to append based on next. */
            cp++;
            switch (*cp) {
            case (char)0:
                /* Append literal percent for percent at EOS. */
                cp--;
            case '%':
                /* %% -> literal percent character. */
                g_string_append_c(fx->scratch, '%');
                break;
            case 'T':
                /* %T -> timestamp */
                air_time_g_string_append(fx->scratch, time(NULL), 
                                         AIR_TIME_SQUISHED);
                break;
            case 'S':
                /* %S -> autoincrementing serial number */
                g_string_append_printf(fx->scratch, "%u", cfg->next_serial++);
                break;
            case 'X':
                /* %X -> autoincrementing serial number in hex */
                g_string_append_printf(fx->scratch, "%08x", cfg->next_serial++);
                break;
            case 'd':
                /* %d -> source directory name */
                if (!decname) {
                    mio_sink_file_pattern_decname(source->name, &decname, 
                                            &dirname, &basename, &extname);
                }
                if (dirname) {
                    g_string_append_printf(fx->scratch, "%s", dirname);
                } else {
                    /* no dirname - source in cwd */
                    g_string_append_printf(fx->scratch, ".");
                }
                break;
            case 's':
                /* %s -> source basename */
                if (!decname) {
                    mio_sink_file_pattern_decname(source->name, &decname, 
                                            &dirname, &basename, &extname);
                }
                if (basename) {
                    g_string_append_printf(fx->scratch, "%s", basename);
                }
                break;
            case 'e':
                /* %e -> source extension */
                if (!decname) {
                    mio_sink_file_pattern_decname(source->name, &decname, 
                                            &dirname, &basename, &extname);
                }
                if (extname) {
                    g_string_append_printf(fx->scratch, "%s", extname);
                }

                break;
            default:
                /* eat unknown % patterns */
                break;
            }
        } else {
            /* Normal character. Copy it. */
            g_string_append_c(fx->scratch, *cp);
        }
    }
    
    /* Clean up decname */
    if (decname) g_free(decname);
    
    /* Copy pattern-generated name to sink */
    sink->name = g_strdup(fx->scratch->str);
}

static gboolean mio_sink_next_file_pattern(
    MIOSource               *source,
    MIOSink                 *sink,
    uint32_t                *flags,
    GError                  **err)
{
    /* Generate name based on pattern */
    mio_sink_file_pattern_to_name(source, sink);
    
    /* Open the file */
    return mio_sink_open_file(sink, flags, err);
}

#define MIO_CLOSE_FILE_ERROR(_action_) {                                    \
    ok = FALSE;                                                             \
    if (!errstr) errstr = g_string_new("I/O error on close:");              \
    g_string_append_printf(errstr, "\nfailed to %s %s: %s",                 \
                            (_action_), sink->name, strerror(errno));       \
}

static gboolean mio_sink_close_file(
    MIOSource               *source,
    MIOSink                 *sink,
    uint32_t                *flags,
    GError                  **err)
{
    MIOSinkFileContext      *fx = (MIOSinkFileContext *)sink->ctx;
    gboolean                ok = TRUE;
    GString                 *errstr = NULL;

    /* Close file pointer or file descriptor as necessary */
    if (sink->vsp_type == MIO_T_FP) {
        if (fclose((FILE *)sink->vsp) < 0)
            MIO_CLOSE_FILE_ERROR("close");
    } else if (sink->vsp_type == MIO_T_FD) {
        if (close(GPOINTER_TO_INT(sink->vsp)) < 0)
            MIO_CLOSE_FILE_ERROR("close");
    }

    /* Delete output file on any error */
    if (*flags & (MIO_F_CTL_ERROR | MIO_F_CTL_TRANSIENT)) {
        if (unlink(sink->name) < 0)
            MIO_CLOSE_FILE_ERROR("delete");
    }
    
    /* Unlock file */
    if (fx->lfd)
    {
        close(fx->lfd);
    }
    if (fx->lpath) unlink(fx->lpath);

    /* Clear file */
    if (fx->lpath) {
        g_free(fx->lpath);
        fx->lpath = NULL;
    }
    if (sink->name) {
        g_free(sink->name);
        sink->name = NULL;
    }
    sink->vsp = NULL;
    
    /* Handle error */
    if (!ok) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_IO, errstr->str);
        g_string_free(errstr, TRUE);
        *flags |= MIO_F_CTL_ERROR;
    }
    
    /* all done */
    return ok;
}

static void mio_sink_free_file(
    MIOSink         *sink)
{
    MIOSinkFileContext      *fx = (MIOSinkFileContext *)sink->ctx;

    if (sink->spec) g_free(sink->spec);
    
    if (fx) {
        if (fx->scratch) g_string_free(fx->scratch, TRUE);
        g_free(fx);
    }
}

static gboolean mio_sink_init_file_inner(
    MIOSink         *sink,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    MIOSinkFn       next_sink,
    gboolean        iterative,
    GError          **err)
{
    /* choose default type */
    if (vsp_type == MIO_T_ANY) vsp_type = MIO_T_FP;

    /* Ensure type is valid */
    if (!(vsp_type == MIO_T_NULL || 
          vsp_type == MIO_T_FD || 
          vsp_type == MIO_T_FP)) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Cannot open file sink: type mismatch");
        return FALSE;        
    }
    
    /* initialize sink */
    sink->spec = g_strdup(spec);
    sink->name = NULL;
    sink->vsp_type = vsp_type;
    sink->vsp = NULL;
    sink->ctx = g_new0(MIOSinkFileContext, 1);
    sink->cfg = cfg;
    sink->next_sink = next_sink;
    sink->close_sink = mio_sink_close_file;
    sink->free_sink = mio_sink_free_file;
    sink->opened = FALSE;
    sink->active = FALSE;
    sink->iterative = iterative;
    
    return TRUE;
}

gboolean mio_sink_init_file_pattern(
    MIOSink         *sink,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{
    /* check that specifier exists */
    if (!spec || !strlen(spec)) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Sink specifier is empty");
        return FALSE;
    }

    /* failover to single */
    if (!strchr(spec, '%')) {
        return mio_sink_init_file_single(sink, spec, vsp_type, cfg, err);
    }
    
    return mio_sink_init_file_inner(sink, spec, vsp_type, cfg,
                                    mio_sink_next_file_pattern, TRUE, err);
}

gboolean mio_sink_init_file_single(
    MIOSink         *sink,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{
    /* check that specifier exists */
    if (!spec || !strlen(spec)) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Sink specifier is empty");
        return FALSE;
    }
    
    /* failover to stdout */
    if (!strcmp(spec, "-")) {
        return mio_sink_init_stdout(sink, spec, vsp_type, cfg, err);
    }
    
    return mio_sink_init_file_inner(sink, spec, vsp_type, cfg,
                                    mio_sink_next_file_single, FALSE, err);
}
