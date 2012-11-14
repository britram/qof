/*
 ** mio_source_file.c
 ** Multiple I/O regular file source, from single file, glob, or directory.
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
#include <airframe/mio_source_file.h>
#include <airframe/mio_stdio.h>

typedef struct _MIOSourceFileEntry {
    char                *path;
    char                *lpath;
} MIOSourceFileEntry;

typedef struct _MIOSourceFileContext {
    GQueue              *queue;
    GMemChunk           *entrychunk;
    GStringChunk        *pathchunk;
    GString             *scratch;
    char                *lpath;
} MIOSourceFileContext;

static MIOSourceFileContext *mio_source_file_context(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    MIOSourceFileContext    *fx = (MIOSourceFileContext *)source->ctx;

    if (!fx) {
        /* create file context on first call */
        fx = g_new0(MIOSourceFileContext, 1);
        fx->queue = g_queue_new();
        source->ctx = fx;
    } else if (!(*flags & MIO_F_OPT_DAEMON) && g_queue_is_empty(fx->queue)) {
        /* queue exists and is empty; not in daemon mode so terminate. */
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_NOINPUT,
                    "End of input");
        *flags |= MIO_F_CTL_TERMINATE;
        return NULL;
   }

    return fx;
}

static void mio_source_file_context_reset(
    MIOSourceFileContext    *fx)
{
    if (fx->entrychunk) g_mem_chunk_destroy(fx->entrychunk);
    fx->entrychunk = g_mem_chunk_new("MIOEntryChunk",
                                     sizeof(MIOSourceFileEntry),
                                     sizeof(MIOSourceFileEntry) * 256,
                                     G_ALLOC_ONLY);
    if (fx->pathchunk) g_string_chunk_free(fx->pathchunk);
    fx->pathchunk = g_string_chunk_new(16384);
}

static MIOSourceFileEntry *mio_source_file_entry_new(
    MIOSourceFileContext    *fx,
    const char              *path,
    uint32_t                flags)
{
    MIOSourceFileEntry      *fent;

    if (flags & MIO_F_OPT_LOCK) {
        /* Generate lock path */
        if (!fx->scratch) (fx->scratch) = g_string_new("");
        g_string_printf(fx->scratch, "%s.lock", path);

        /* Skip files locked at queue time */
        if (g_file_test(fx->scratch->str,G_FILE_TEST_IS_REGULAR)) return NULL;
    }

    /* No lock contention right now; create the entry. */
    fent = g_mem_chunk_alloc0(fx->entrychunk);
    fent->path = g_string_chunk_insert(fx->pathchunk, path);
    if (flags & MIO_F_OPT_LOCK) {
        fent->lpath = g_string_chunk_insert(fx->pathchunk, fx->scratch->str);
    }

    return fent;
}

static gboolean mio_source_next_file_queue(
    MIOSource               *source,
    MIOSourceFileContext    *fx,
    uint32_t                *flags,
    GError                  **err)
{
    int                     fd;
    MIOSourceFileEntry      *fent;

    while (1) {
        /* Attempt to dequeue a file entry */
        if (!(fent = g_queue_pop_tail(fx->queue))) {
            /* Queue is empty. We're done. */
            g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_NOINPUT,
                        "End of input");
            *flags |= MIO_F_CTL_POLL;
            return FALSE;
        }

        /* Attempt lock */
        if (fent->lpath) {
            fd = open(fent->lpath, O_WRONLY | O_CREAT | O_EXCL, 0664);
            if (fd < 0) continue;
            close(fd);
        }

        /* Verify existence */
        if (!g_file_test(fent->path, G_FILE_TEST_IS_REGULAR)) {
            /* file not here; unlock it */
            if (fent->lpath) unlink(fent->lpath);
            continue;
        }

        /* We own the file. Store paths from the queue entry */
        source->name = fent->path;
        fx->lpath = fent->lpath;

        /* Now open the file as necessary */
        if (source->vsp_type != MIO_T_NULL) {
            /* Not a null type source. Open the file. */
            fd = open(fent->path, O_RDONLY, 0664);
            if (fd < 0) {
                /* File open failed. Unlock and return error. */
                g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_IO,
                            "Couldn't open file %s for reading: %s",
                            fent->path, strerror(errno));
                *flags |= MIO_F_CTL_ERROR;
                if (fent->lpath) unlink(fent->lpath);
                return FALSE;
            }

            /* Determine how to store opened file */
            if (source->vsp_type == MIO_T_FP) {
                /* As file handle. fdopen should never fail here. */
                source->vsp = fdopen(fd, "r");
                g_assert(source->vsp);
            } else {
                /* As file descriptor. Yay Casting! */
                source->vsp = GINT_TO_POINTER(fd);
            }
        }

        /* Done */
        return TRUE;
    }
}

gboolean mio_source_next_file_dir(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    MIOSourceFileContext    *fx = NULL;
    MIOSourceFileEntry      *fent = NULL;
    uint32_t                fcount = 0, dnamlen = 0;
    DIR                     *dir = NULL;
    struct dirent           *dirent = NULL;

    /* Handle queue empty boundary conditions for non-daemon mode. */
    if (!(fx = mio_source_file_context(source, flags, err))) return FALSE;

    /* Valid queue. Ensure there's something in it. */
    if (g_queue_is_empty(fx->queue)) {

        /* Reset file context */
        mio_source_file_context_reset(fx);

        /* Open directory */
        if (!(dir = opendir(source->spec))) {
            g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_IO,
                        "Could not open directory %s: %s",
                        source->spec, strerror(errno));
            *flags |= MIO_F_CTL_ERROR;
            return FALSE;
        }

        /* Iterate over directory entries, enqueueing. */
        while ((dirent = readdir(dir))) {
            dnamlen = strlen(dirent->d_name);

            /* Skip lockfiles */
            if (!strcmp(".lock", &(dirent->d_name[dnamlen]))) {
                continue;
            }

            /* Skip non-regular files */
#if HAVE_STRUCT_DIRENT_D_TYPE
            if (dirent->d_type != DT_REG) {
                continue;
            }
#endif
            /* Create a new file entry; skip on lock contention. */
            if (!(fent = mio_source_file_entry_new(fx, dirent->d_name,
                                                   *flags))) {
                continue;
            }

            /* Enqueue new entry */
            g_queue_push_head(fx->queue, fent);
            ++fcount;
        }

        /* Close directory */
        if (closedir(dir) < 0) {
            g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_IO,
                        "Could not close directory %s: %s",
                        source->spec, strerror(errno));
            *flags |= MIO_F_CTL_ERROR;
            return FALSE;
        }
    }

    /* Filled queue if possible. Dequeue and open next file. */
    return mio_source_next_file_queue(source, fx, flags, err);
}

gboolean mio_source_next_file_glob(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    MIOSourceFileContext    *fx = NULL;
    MIOSourceFileEntry      *fent = NULL;
    glob_t                  gbuf;
    int                     grc, i;

    /* Handle queue empty boundary conditions for non-daemon mode. */
    if (!(fx = mio_source_file_context(source, flags, err))) return FALSE;

    /* Valid queue. Ensure there's something in it. */
    if (g_queue_is_empty(fx->queue)) {

        /* Reset file context */
        mio_source_file_context_reset(fx);

        /* Evaluate glob expression */
        grc = glob(source->spec, 0, NULL, &gbuf);
        if (grc == GLOB_NOSPACE) {
            g_error("Out of memory: glob allocation failure");
        }
#ifdef GLOB_NOMATCH
        /* HaX0riffic! Simulate behavior without NOMATCH where we have it. */
        else if (grc == GLOB_NOMATCH) {
            gbuf.gl_pathc = 0;
            gbuf.gl_pathv = NULL;
        }
#endif

        /* Iterate over glob paths, enqueueing. */
        for (i = 0; i < gbuf.gl_pathc; i++) {
            /* Skip non-regular files */
            if (!g_file_test(gbuf.gl_pathv[i],G_FILE_TEST_IS_REGULAR)) {
                continue;
            }

            /* Skip lockfiles */
            if (!strcmp(".lock", gbuf.gl_pathv[i]
                                 + strlen(gbuf.gl_pathv[i]) - 5)) {
                continue;
            }

            /* Create a new file entry; skip on lock contention. */
            if (!(fent = mio_source_file_entry_new(fx, gbuf.gl_pathv[i],
                                                   *flags))) {
                continue;
            }

            /* Enqueue new entry */
            g_queue_push_head(fx->queue, fent);
        }

        /* Free glob buffer */
        globfree(&gbuf);
    }

    /* Filled queue if possible. Dequeue and open next file. */
    return mio_source_next_file_queue(source, fx, flags, err);
}

gboolean mio_source_next_file_single(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    MIOSourceFileContext    *fx = NULL;
    MIOSourceFileEntry      *fent = NULL;

    /* Handle queue empty boundary conditions for non-daemon mode. */
    if (!(fx = mio_source_file_context(source, flags, err))) return FALSE;

    /* Valid queue. Ensure there's something in it. */
    if (g_queue_is_empty(fx->queue)) {

        /* Reset file context */
        mio_source_file_context_reset(fx);

        /* Add single entry */
        if ((fent = mio_source_file_entry_new(fx, source->spec, *flags))) {
            g_queue_push_head(fx->queue, fent);
        }
    }

    /* Filled queue if possible. Dequeue and open next file. */
    return mio_source_next_file_queue(source, fx, flags, err);
}

#define MIO_CLOSE_FILE_ERROR(_action_) {                                    \
    ok = FALSE;                                                             \
    if (!errstr) errstr = g_string_new("I/O error on close:");              \
    g_string_append_printf(errstr, "\nfailed to %s %s: %s",                 \
                            (_action_), source->name, strerror(errno));     \
}

gboolean mio_source_close_file(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    MIOSourceFileContext    *fx = (MIOSourceFileContext *)source->ctx;
    MIOSourceFileConfig     *cfg = (MIOSourceFileConfig *)source->cfg;
    char                    *ddir = NULL, *dbase = NULL;
    gboolean                ok = TRUE;
    GString                 *errstr = NULL;

    /* Close file pointer or file descriptor as necessary */
    if (source->vsp_type == MIO_T_FP) {
        if (fclose((FILE *)source->vsp) < 0)
            MIO_CLOSE_FILE_ERROR("close");
    } else if (source->vsp_type == MIO_T_FD) {
        if (close(GPOINTER_TO_INT(source->vsp)) < 0)
            MIO_CLOSE_FILE_ERROR("close");
    }

    /* Determine move destination directory */
    if (*flags & MIO_F_CTL_ERROR) {
        /* Error. Move to fail directory. */
        ddir = cfg->faildir;
    } else if (*flags & MIO_F_CTL_TRANSIENT) {
        /* Transient error. Do not move. */
        ddir = NULL;
    } else {
        /* No error. Move to next directory. */
        ddir = cfg->nextdir;
    }

    /* Do move or delete */
    if (ddir) {
        if (*ddir) {
            /* Create scratch string if necessary */
            if (!fx->scratch) fx->scratch = g_string_new("");
            /* Calculate move destination path */
            dbase = g_path_get_basename(source->name);
            g_string_printf(fx->scratch, "%s/%s", ddir, dbase);
            g_free(dbase);
            /* Do link */
            if (link(source->name, fx->scratch->str) < 0)
                MIO_CLOSE_FILE_ERROR("move");
        }

        /* Do delete */
        if (unlink(source->name) < 0)
            MIO_CLOSE_FILE_ERROR("delete");
    }

    /* Unlock file */
    if (fx->lpath) unlink(fx->lpath);

    /* Clear file */
    fx->lpath = NULL;
    source->name = NULL;
    source->vsp = NULL;

    /* Handle error */
    if (!ok) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_IO, "%s", errstr->str);
        g_string_free(errstr, TRUE);
        *flags |= MIO_F_CTL_ERROR;
    }

    /* all done */
    return ok;
}

void mio_source_free_file(
    MIOSource       *source)
{
    MIOSourceFileContext    *fx = (MIOSourceFileContext *)source->ctx;

    if (source->spec) g_free(source->spec);

    if (fx) {
        if (fx->queue) g_queue_free(fx->queue);
        if (fx->entrychunk) g_mem_chunk_destroy(fx->entrychunk);
        if (fx->pathchunk) g_string_chunk_free(fx->pathchunk);
        if (fx->scratch) g_string_free(fx->scratch, TRUE);
        g_free(fx);
    }
}

static gboolean mio_source_init_file_inner(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    MIOSourceFn     next_source,
    GError          **err)
{
    /* choose default type */
    if (vsp_type == MIO_T_ANY) vsp_type = MIO_T_FP;

    /* initialize file source */
    source->spec = g_strdup(spec);
    source->name = NULL;
    source->vsp_type = vsp_type;
    source->vsp = NULL;
    source->ctx = NULL;
    source->cfg = cfg;
    source->next_source = next_source;
    source->close_source = mio_source_close_file;
    source->free_source = mio_source_free_file;
    source->opened = FALSE;
    source->active = FALSE;

    /* Ensure type is valid */
    if (!(vsp_type == MIO_T_NULL ||
          vsp_type == MIO_T_FD ||
          vsp_type == MIO_T_FP)) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Cannot open file source: type mismatch");
        return FALSE;
    }

    return TRUE;
}

gboolean mio_source_init_file_dir(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{
    /* check that specifier exists */
    if (!spec || !strlen(spec)) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Source specifier is empty");
        return FALSE;
    }

    /* check that specifier is an accessible directory */
    if (!g_file_test(spec, G_FILE_TEST_IS_DIR)) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Source specifier %s is not a directory", spec);
        return FALSE;
    }

    /* initialize source */
    return mio_source_init_file_inner(source, spec, vsp_type, cfg,
                                      mio_source_next_file_dir, err);
}

gboolean mio_source_init_file_glob(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{
    /* check that specifier exists */
    if (!spec || !strlen(spec)) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Source specifier is empty");
        return FALSE;
    }

    /* failover to single */
    if (!strchr(spec, '*') && !strchr(spec, '?') && !strchr(spec, '[')) {
        return mio_source_init_file_single(source, spec, vsp_type, cfg, err);
    }

    /* initialize source */
    return mio_source_init_file_inner(source, spec, vsp_type, cfg,
                                      mio_source_next_file_glob, err);
}

gboolean mio_source_init_file_single(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{
    /* check that specifier exists */
    if (!spec || !strlen(spec)) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Source specifier is empty");
        return FALSE;
    }

    /* failover to stdin */
    if (!strcmp(spec, "-")) {
        return mio_source_init_stdin(source, spec, vsp_type, cfg, err);
    }

    /* initialize source */
    return mio_source_init_file_inner(source, spec, vsp_type, cfg,
                                      mio_source_next_file_single, err);
}
