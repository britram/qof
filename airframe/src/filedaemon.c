/*
** filedaemon.c
** Invokes yaf, naf, etc. as a file daemon process
**
** ------------------------------------------------------------------------
** Copyright (C) 2007-2011 Carnegie Mellon University. All Rights Reserved.
** ------------------------------------------------------------------------
** Authors: Tony Cebzanov <tonyc@cert.org>
** ------------------------------------------------------------------------
** GNU General Public License (GPL) Rights pursuant to Version 2, June 1991
** Government Purpose License Rights (GPLR) pursuant to DFARS 252.227-7013
** ------------------------------------------------------------------------
*/

#define _AIRFRAME_SOURCE_
#include <airframe/autoinc.h>
#include <airframe/airopt.h>
#include <airframe/logconfig.h>

char        *fd_inspec          = NULL;
char        *fd_outspec         = NULL;
char        *fd_nextdir         = NULL;
char        *fd_faildir         = NULL;
char        *fd_outext          = NULL;
uint32_t     fd_poll_delay      = 30;
gboolean     fd_lock            = FALSE;
gsize        fd_bufsize         = 1024;
char        *fd_pidfile         = NULL;
pid_t        fd_pid             = 0;
gboolean     fd_nodaemon        = FALSE;

AirOptionEntry fd_options[]  = {
    AF_OPTION( "in", 'i', 0, AF_OPT_TYPE_STRING, &fd_inspec,
               "Input specifier", "inspec" ),
    AF_OPTION( "out", 'o', 0, AF_OPT_TYPE_STRING, &fd_outspec,
               "Output specifier", "outspec" ),
    AF_OPTION( "nextdir", 'n', 0, AF_OPT_TYPE_STRING, &fd_nextdir,
               "Directory to move good input to (or 'delete')", "dir" ),
    AF_OPTION( "faildir", 'x', 0, AF_OPT_TYPE_STRING, &fd_faildir,
               "Directory to move failed input to (or 'delete')", "dir" ),
    AF_OPTION( "extension", 'e', 0, AF_OPT_TYPE_STRING, &fd_outext,
               "Extension to use for output files", "extension" ),
    AF_OPTION( "poll", 'p', 0, AF_OPT_TYPE_INT, &fd_poll_delay,
               "Polling delay in seconds", "sec" ),
    AF_OPTION( "lock", 'k', 0, AF_OPT_TYPE_NONE, &fd_lock,
               "Use exclusive .lock files for concurrency", NULL ),
    AF_OPTION( "pidfile", 'P', 0, AF_OPT_TYPE_STRING, &fd_pidfile,
               "A filename to write the daemon's pid to", NULL ),
    AF_OPTION( "no-daemon", (char)0, 0, AF_OPT_TYPE_NONE, &fd_nodaemon,
               "do not daemonize", NULL ),
    AF_OPTION_END
};


static void parse_options(
    int              *argc,
    char            **argv[]) {

    AirOptionCtx    *aoctx = NULL;

    aoctx = air_option_context_new("", argc, argv, fd_options);
    logc_add_option_group(aoctx, "filedaemon", VERSION);

    air_option_context_set_help_enabled(aoctx);

    air_option_context_parse(aoctx);
}

typedef struct _fd_write_data {
    GIOChannel   *infile;
    gchar        *buf;
} fd_write_data_t;

typedef struct _fd_read_data {
    GIOChannel   *outfile;
    gchar        *buf;
    GMainLoop    *loop;
} fd_read_data_t;

static gboolean write_to_child(
    GIOChannel  *child_stdin,
    GIOCondition condition,
    gpointer     data)
{
    GIOStatus      ret;
    GError        *err    = NULL;
    gsize          bytes_read, bytes_written;
    GIOChannel    *infile =  ((fd_write_data_t *)data)->infile;
    gchar         *buf    =  ((fd_write_data_t *)data)->buf;
    /** number of attempts for temporarily busy resource */
    unsigned char stillRetry = 10;

    static int br         = 0;

    if (condition & G_IO_HUP)
        g_critical("Write end of pipe died!");
    if (condition & G_IO_ERR)
        g_critical("Error writing to child process");

    while(stillRetry)
    {
        ret = g_io_channel_read_chars(infile, buf, fd_bufsize,
                                      &bytes_read,
                                      &err);
        switch (ret) {
          case G_IO_STATUS_ERROR:
            g_critical("Error reading: %s", err->message);
            break;
          case G_IO_STATUS_EOF:
            g_debug("eof from infile");
            g_io_channel_shutdown(child_stdin, TRUE, &err);
            return FALSE;
          case G_IO_STATUS_AGAIN:
            g_debug("resource temporarily busy");
            stillRetry--;
            continue;
          default:
            break;
        }
        break;
    }

    br += bytes_read;

    /* g_debug("Read %u bytes from input file (total: %lu).", bytes_read, br);*/

    ret = g_io_channel_write_chars(child_stdin, buf, bytes_read, &bytes_written,
                                   &err);

    if (ret == G_IO_STATUS_ERROR)
        g_critical("Error writing: %s", err->message);

    /* g_debug("Wrote %u bytes to child.", bytes_written); */
    return TRUE;
}

static gboolean read_from_child(
    GIOChannel  *child_stdout,
    GIOCondition condition,
    gpointer     data)
{
    GIOStatus         ret;
    GError           *err     = NULL;
    gsize             bytes_read, bytes_written;
    GIOChannel       *outfile = ((fd_read_data_t *)data)->outfile;
    gchar            *buf     =  ((fd_read_data_t *)data)->buf;
    GMainLoop        *loop    = ((fd_read_data_t *)data)->loop;
    /** number of times to retry a temporary busy read */
    unsigned char    stillRetry = 10;

    if (condition & G_IO_HUP)
        g_critical("Read end of pipe died!");
    if (condition & G_IO_ERR)
        g_critical("Error reading from child process");

    while (stillRetry) {
        ret = g_io_channel_read_chars(child_stdout, buf, fd_bufsize,
                                      &bytes_read, &err);
        switch (ret) {
          case G_IO_STATUS_ERROR:
            g_critical("Error reading: %s", err->message);
            break;
          case G_IO_STATUS_EOF:
            g_debug("EOF from child (read %u)", (unsigned int)bytes_read);
            g_io_channel_shutdown(outfile, TRUE, &err);
            g_io_channel_shutdown(child_stdout, TRUE, &err);
            g_main_loop_quit(loop);
            return FALSE;
          case G_IO_STATUS_AGAIN:
            g_debug("resource temporarily busy");
            stillRetry--;
            continue;
          default:
            break;
        }
        break;
    }

    /* g_debug("Read %u bytes from child", bytes_read); */

    ret = g_io_channel_write_chars(outfile, buf, bytes_read, &bytes_written,
                                   &err);
    if (ret == G_IO_STATUS_ERROR)
        g_critical("Error writing: %s", err->message);

    /* g_debug("Wrote %u bytes to output file.", bytes_written); */
    return TRUE;
}


static void on_child_exit(
    GPid     child_pid,
    gint     status,
    gpointer data)
{
    char *infile  = (char *) data;
    char *destdir = NULL;

    g_message("pid %lu exited with status %d", (gulong) child_pid, status);

#ifdef G_OS_UNIX
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == EXIT_SUCCESS) {
            g_debug("pid %lu returned success", (gulong) child_pid);
            if (g_ascii_strcasecmp(fd_nextdir, "delete"))
                destdir = fd_nextdir;
        } else {
            g_warning("pid %lu returned error status %d", (gulong) child_pid,
                      WEXITSTATUS(status));
            if (g_ascii_strcasecmp(fd_faildir, "delete"))
                destdir = fd_faildir;
        }
    } else if (WIFSIGNALED(status)) {
        g_critical("pid %lu terminated with signal %d\n",
                   (gulong) child_pid, WTERMSIG(status));
        if (g_ascii_strcasecmp(fd_faildir, "delete"))
            destdir = fd_faildir;
    } else {
        g_critical("pid %lu terminated", (gulong) child_pid);
    }
#endif /* G_OS_UNIX */

    /* Do move or delete */
    if (destdir) {
        if (*destdir) {
            GString *destpath = g_string_new("");
            char    *dbase    = NULL;
            /* Calculate move destination path */
            dbase = g_path_get_basename(infile);
            g_string_printf(destpath, "%s/%s", destdir, dbase);
            if (dbase) free(dbase);
            /* Do link */
            g_message("moving %s -> %s", infile, destpath->str);
            if (link(infile, destpath->str) < 0)
                g_critical(
                    "error moving input file to destination directory: %s",
                    strerror(errno));
            g_string_free(destpath, TRUE);
        }
    }

    /* Do delete */
    if (unlink(infile) < 0)
        g_critical("error deleting input file");

    g_spawn_close_pid(child_pid);
}


void fd_lock_file(
    char *filename)
{
    GString *lockpath = NULL;
    int      fd       = -1;

    lockpath = g_string_new("");

    g_string_printf(lockpath, "%s.lock", filename);

    /* Attempt lock */
    fd = open(lockpath->str, O_WRONLY | O_CREAT | O_EXCL, 0664);
    if (fd < 0)
        goto done;
    close(fd);

    /* Verify existence */
    if (!g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
        /* file not here; unlock it */
        if (lockpath->str) unlink(lockpath->str);
    }

  done:
    if (lockpath) g_string_free(lockpath, TRUE);
}

void fd_unlock_file(
    char *filename)
{
    GString *lockpath = NULL;

    lockpath = g_string_new("");

    g_string_printf(lockpath, "%s.lock", filename);

    /* Unlock file */
    if (lockpath) {
        unlink(lockpath->str);
        if (lockpath) g_string_free(lockpath, TRUE);
    }
}

gboolean daemonize(
    void)
{
    /* fork */
    if (fork()) exit(0);

    /* dissociate from controlling terminal */
    if (setsid() < 0) {
        g_critical("setsid() failed: %s", strerror(errno));
        return FALSE;
    }

    /* redirect stdio */
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    fd_pid = getpid();
    if (fd_pidfile) {
        FILE *pidfile = fopen(fd_pidfile,"w");
        if (!pidfile) {
            g_critical("could not write pidfile");
            goto end;
        }
        fprintf(pidfile, "%d\n", fd_pid);
        fclose(pidfile);
    }
  end:

    return TRUE;
}

int main(
    int              argc,
    char            *argv[])
{

    glob_t                   gbuf;
    int                      grc, i;
    GString                 *cmd;
    GError                  *err = NULL;

    GMainLoop *loop;

    GPtrArray    *child_args     = NULL;

    /* parse options */
    parse_options(&argc, &argv);

    /* set up logging */
    if (!logc_setup(&err)) {
        air_opterr("%s", err->message);
    }

    if (fd_nextdir == NULL) {
        air_opterr("The --nextdir switch is required");
    }

    if (fd_faildir == NULL) {
        air_opterr("The --faildir switch is required");
    }

    child_args = g_ptr_array_sized_new(64);
    for (i=1; i < argc; i++) {
        /* Double dash indicates end of filedaemon's arguments */
        if (!strncmp(argv[i], "--", strlen(argv[i])) )
            continue;
        g_ptr_array_add(child_args, g_strdup(argv[i]));
    }
    g_ptr_array_add(child_args, NULL);

    cmd  = g_string_new("");

    loop = g_main_loop_new(NULL, FALSE);

    /* We need an input glob */
    if (!fd_inspec) {
        air_opterr("Input glob must be specified");
    }

    /* If an output destination is provided, make sure it's a directory */
    if (fd_outspec && !g_file_test(fd_outspec, G_FILE_TEST_IS_DIR )) {
        air_opterr("Output is not a directory");
    }

    /* Options check out; daemonize */
    if (!fd_nodaemon) {
        if (!daemonize()) {
            goto end;
        }
    }

    while (1) {
        /* Evaluate glob expression */
        grc = glob(fd_inspec, 0, NULL, &gbuf);

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
            char      **child_envp            = {NULL};
            GError     *child_err             = NULL;

            GString    *filename_in           = NULL;
            GString    *filename_out          = NULL;
            GString    *filename_lock         = NULL;

            GIOChannel *file_in               = NULL;
            GIOChannel *file_out              = NULL;

            GIOChannel *child_stdin           = NULL;
            gint        child_stdin_fd        = -1;

            GIOChannel *child_stdout          = NULL;
            gint        child_stdout_fd       = -1;

            GPid          child_pid;
            int         len;

            fd_read_data_t  read_data;
            fd_write_data_t write_data;

            filename_in = g_string_new(gbuf.gl_pathv[i]);

            /* Skip non-regular files */
            if (!g_file_test(filename_in->str, G_FILE_TEST_IS_REGULAR) ) {
                continue;
            }

            /* Skip lockfiles */
            if (!strcmp(".lock", filename_in->str
                        + strlen(filename_in->str) - 5))
            {
                continue;
            }

            /* Generate lock path */
            if (!filename_lock) filename_lock = g_string_new("");
            g_string_printf(filename_lock, "%s.lock", filename_in->str);

            /* Skip files locked at queue time */
            if (g_file_test(filename_lock->str, G_FILE_TEST_IS_REGULAR)) {
                g_debug("file %s is locked", filename_in->str);
                continue;
            }

            if (fd_lock) {
                fd_lock_file(filename_in->str);
            }

            file_in = g_io_channel_new_file(filename_in->str, "r", &err);

            if (file_in == NULL) {
                g_critical("Cannot open input file!");
            }

            g_io_channel_set_encoding(file_in, NULL, &err);

            if (err) {
                g_critical("error setting input encoding!");
            }

            g_io_channel_set_buffer_size(file_in, fd_bufsize);

            filename_out = g_string_new("");

            if (fd_outspec == NULL) {
                g_string_printf(filename_out, "%s", gbuf.gl_pathv[i]);
            } else {
                g_string_printf(filename_out, "%s/%s", fd_outspec,
                                gbuf.gl_pathv[i]);
            }

            len  = filename_out->len;

            if (g_strrstr(filename_out->str, ".")) {
                while (len-- > 0
                       && !g_str_has_suffix(filename_out->str, ".") )
                {
                    g_string_set_size(filename_out, filename_out->len - 1);
                }
                g_string_set_size(filename_out, filename_out->len - 1);
            }
            if (fd_outext) {
                g_string_append_printf(filename_out, ".%s", fd_outext);
            } else {
                g_string_append(filename_out, ".out");
            }

            g_message("%d: %s -> %s", i, filename_in->str, filename_out->str);

            file_out = g_io_channel_new_file(filename_out->str, "w", &err);

            if (file_out == NULL) {
                g_error("Cannot open output file!");
            }

            g_io_channel_set_encoding(file_out, NULL, &err);

            if (err) {
                g_error("error setting output encoding!");
            }

            g_io_channel_set_buffer_size(file_out, fd_bufsize);

            if (!g_spawn_async_with_pipes(".",
                                          (gchar **) child_args->pdata,
                                          child_envp,
                                          G_SPAWN_SEARCH_PATH |
                                          G_SPAWN_DO_NOT_REAP_CHILD,
                                          NULL,
                                          NULL,
                                          &child_pid,
                                          &child_stdin_fd,
                                          &child_stdout_fd,
                                          NULL,
                                          &child_err))
            {
                g_error("error spawning process: %s",
                        (child_err && child_err->message ? child_err->
                         message : "unknown error"));
            }
            g_debug("spawned process %d", i);

            /* Watch for process exit status */
            g_child_watch_add(child_pid, on_child_exit, filename_in->str);

            child_stdin = g_io_channel_unix_new(child_stdin_fd);

            if (child_stdin == NULL) {
                g_error("Cannot open child stdin!");
            }

            g_io_channel_set_encoding(child_stdin, NULL, &err);

            if (err) {
                g_error("error setting child stdin encoding!");
            }

            g_io_channel_set_buffer_size(child_stdin, fd_bufsize);

            child_stdout = g_io_channel_unix_new(child_stdout_fd);

            if (child_stdout == NULL) {
                g_error("Cannot open child stdout!");
            }

            g_io_channel_set_encoding(child_stdout, NULL, &err);

            if (err) {
                g_error("error setting child stdout encoding!");
            }


            g_io_channel_set_buffer_size(child_stdout, fd_bufsize);


            write_data.infile = file_in;
            write_data.buf    = g_malloc(g_io_channel_get_buffer_size(file_in));

            if (write_data.buf == NULL) {
                g_error("error allocating file_in buffer");
            }

            if (!g_io_add_watch(child_stdin,  G_IO_OUT | G_IO_PRI | G_IO_HUP |
                                G_IO_ERR, write_to_child, &write_data))
                g_error("Cannot add watch on GIOChannel!");

            read_data.outfile = file_out;
            read_data.loop    = loop;
            read_data.buf     = g_malloc(g_io_channel_get_buffer_size(file_out));

            if (write_data.buf == NULL) {
                g_error("error allocating file_in buffer");
            }

            if (!g_io_add_watch(child_stdout, G_IO_IN | G_IO_PRI | G_IO_HUP |
                                G_IO_ERR, read_from_child, &read_data))
                g_error("Cannot add watch on GIOChannel!");

            g_main_loop_run(loop);

            if (fd_lock) {
                fd_unlock_file(filename_in->str);
            }

            if (read_data.buf) {
                g_free(read_data.buf);
            }
            if (write_data.buf) {
                g_free(write_data.buf);
            }
        }
        sleep(fd_poll_delay);
    }

  end:

    return 0;
}
