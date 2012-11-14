/*
 ** mio.h
 ** Multiple I/O configuration and routing support for file and network daemons
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2006-2011 Carnegie Mellon University. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell
 **          Tony Cebzanov <tonyc@cert.org>
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
 * Airframe Multiple I/O Support. Supplies a framework for building 
 * applications that perform record-oriented processing on a wide variety of
 * low-level input and output objects.
 *
 * MIO applications read their input from an MIOSource and write it to an
 * MIOSink. Sources and sinks are iterable; in other words, an application 
 * will generally have a single MIOSource which describes all the input it 
 * will read during its run, and which at any given time has a current source
 * pointer, referring to the currently open file, socket, etc. MIOSource does
 * not manage reading of bytes or records from the current source pointer; it
 * simply hands this pointer (a file descriptor, file pointer, etc.) to the 
 * application and allows the application to handle its own low-level I/O.
 * MIOSink works similarly.
 *
 * Once an application has a source and a sink, it creates an MIOAppDriver 
 * containing five functions - one each to open and close the source, one 
 * each to open and close the sink, and one to process a single record from 
 * source to sink. These functions are called by mio_dispatch(), which is in
 * turn called repeatedly by mio_dispatch_loop() to process all the records in
 * each source described by the MIOSource. Application drivers may set flags 
 * (MIO_F_CTL constants) to control the closing of sinks and sources, the 
 * reporting of errors, and termination of the application.
 *
 * Applications may create their own sources and sinks using the various
 * initialization functions available in each source or sink's header file;
 * however, it is more common to use the facilities in mio_config.h to allow 
 * the user to configure input and output.
 */

/* idem */
#ifndef _AIRFRAME_MIO_H_
#define _AIRFRAME_MIO_H_
#include <airframe/autoinc.h>

/** GError domain for MIO errors */
#define MIO_ERROR_DOMAIN        g_quark_from_string("airframeMIO")
/** 
 * Multiple errors occurred. Usually occurs when attempt to clean up after 
 * an error fails itself. See the error message for detailed information.
 */
#define MIO_ERROR_MULTIPLE      1
/** 
 * Illegal argument. The user supplied an illegal argument combination, or 
 * referred to an inaccessible or nonexistant path.
 */
#define MIO_ERROR_ARGUMENT      2
/** An unspecified I/O error occurred. */
#define MIO_ERROR_IO            3
/** A connection error occurred; a connection could not be opened or bound. */
#define MIO_ERROR_CONN          4
/** No input is presently available. */
#define MIO_ERROR_NOINPUT       5
/** Output path is locked. */
#define MIO_ERROR_LOCK          6
/** Requested function is not implemented in this build of Airframe. */
#define MIO_ERROR_IMPL          7

/** 
 * MIO dispatch function control flag signaling a non-retryable error. 
 * This simply serves to differentiate error conditions from "normal"
 * exception conditions. Set by low-level next, application open, or 
 * application process functions. These functions must also set
 * MIO_F_SOURCECLOSE or MIO_F_SINKCLOSE if the source or sink must 
 * also be closed. Should be checked by the mio_dispatch() caller to 
 * detect non-retryable errors.
 */
#define MIO_F_CTL_ERROR         0x00000001

/** 
 * MIO dispatch function control flag signaling a transient error. Transient
 * errors can be retried; Applications may begin retry backoff on transient 
 * errors to avoid using resources with frequent retry. Set by low-level next,
 * application open, or application process functions. These functions must
 * also set MIO_F_SOURCECLOSE or MIO_F_SINKCLOSE if the source or sink must 
 * also be closed. Should by checked by mio_dispatch() caller to detect
 * transient errors.
 */
#define MIO_F_CTL_TRANSIENT     0x00000002

/**
 * MIO dispatch function control flag signaling that the source should be
 * closed. Set on error by low-level next, application open, or application 
 * process functions, or on EOF or other close condition by application process
 * function. Causes mio_dispatch() to close the source.
 */
#define MIO_F_CTL_SOURCECLOSE   0x00000004

/**
 * MIO dispatch function control flag signaling that the sink should be
 * closed. Set on error by low-level next, application open, or application 
 * process functions, by mio_dispatch() if MIO_F_CTL_SOURCECLOSE and 
 * MIO_F_OPT_SINKLINK are set, or on other close condition (e.g. rotation delay
 * timeout) by application process function. Causes mio_dispatch() to close the
 * sink.
 */
#define MIO_F_CTL_SINKCLOSE     0x00000008

/**
 * MIO dispatch function control flag signaling that the application should
 * terminate. Set by low-level next, application open, or application process
 * functions. 
 */
#define MIO_F_CTL_TERMINATE     0x00000010

/**
 * MIO dispatch function control flag signaling that no input is available
 * from the source, and the source cannot block waiting for new input.
 * mio_dispatch() caller should wait and try again. Set by low-level next
 * function.
 */
#define MIO_F_CTL_POLL          0x00000020

/**
 * MIO dispatch function control flag mask. mio_dispatch() clears each flag 
 * in this mask at the start of the call. Flags in this mask have a special 
 * meaning to mio_dispatch(), and may not be used by applications for their 
 * own purposes.
 */
#define MIO_F_CTL_MASK          0x0000003f

/**
 * MIO option flag signaling locking should be used if available. Locking is 
 * used to protect concurrent access to sources and sinks by multiple 
 * MIO-based applications. Set by mio_dispatch() caller, used by low-level 
 * source and sink next and close functions. Ignored by sources and sinks for
 * which locking is not available or semantically meaningful; callers should
 * be aware that setting this flag does NOT guarantee exclusive access among
 * processes when using such sources and sinks. Set by mio_config_source() 
 * if --lock is present.
 */
#define MIO_F_OPT_LOCK          0x00008000

/**
 * MIO option flag signaling sources should operate in daemon mode. Without
 * this option, MIO low-level next will fail and set MIO_F_CTL_TERMINATE
 * once all the available data covered by the input specifier has been
 * processed once. With this option, the input specifier will be evaluated
 * continually, and MIO itself will never terminate the application with 
 * MIO_F_CTL_TERMINATE; if no input is available, mio_dispatch will fail 
 * and set MIO_F_CTL_POLL instead. Set by mio_config_source() if 
 * daec_is_daemon() is TRUE.
 */
#define MIO_F_OPT_DAEMON        0x00004000

/**
 * MIO option flag signaling one-to-one mapping between source and sink.
 * If this option is set, mio_dispatch() will set MIO_F_CTL_SINKCLOSE if 
 * MIO_F_CTL_SOURCECLOSE is set. Applications should set this flag if they
 * are not managing multiple sink closure in their process function, and each
 * input logically maps to a single output; however, mio_config_sink() may
 * override this option for single file output.
 */
#define MIO_F_OPT_SINKLINK      0x00002000

/**
 * MIO option flag mask. Flags in this mask are set by the application and
 * undisturbed by dispatch operations. Flags in this mask have a special 
 * meaning to mio_dispatch(), and may not be used by applications for their 
 * own purposes.
 */
#define MIO_F_OPT_MASK          0x0000ffc0

/**
 * Enumeration of low-level I/O object types MIOSource and MIOSink can manage.
 */
typedef enum _MIOType {
    /**
     * As an argument to a source init function, directs the source to use its
     * default vsp_type. Not valid as an actual vsp_type.
     */
    MIO_T_ANY = 0,
    /**
     * Source or sink pointer is managed by the application driver; used to
     * keep the MIO dispatch loop but with no separation between application
     * and low-level I/O (e.g., for AirDBC intergation)
     */
    MIO_T_APP = 1,
    /**
     * Source or sink pointer is NULL. Used to suppress actual opening of 
     * sources, in cases where the application open/close routines must do so
     * (e.g., external APIs which require file paths, not file descriptors or 
     * pointers.
     */
    MIO_T_NULL = 2,
    /**
     * Source or sink pointer is a non-socket file descriptor. 
     * Used for unbuffered I/O, when an external API will provide its own
     * buffering.
     */
    MIO_T_FD = 3,
    /**
     * Source or sink pointer is a FILE pointer. Used for buffered I/O,
     * especially for files.
     */
    MIO_T_FP = 4,
    /**
     * Source pointer is a pointer to a pcap_t libpcap context. Used for 
     * packet capture from live interface or pcap dumpfile. Not valid on
     * sinks; MIO does not support dumpfile output natively.
     */
    MIO_T_PCAP = 5,
    /**
     * Source or sink pointer is a connect()ed datagram socket. Use for 
     * reading or writing UDP packets.
     */
    MIO_T_SOCK_DGRAM = 6,
    /**
     * Source or sink pointer is a connect()ed stream socket. Use for
     * reading or writing TCP streams.
     */
    MIO_T_SOCK_STREAM = 7,
    /**
     * Sink pointer is a pointer to an array of MIOSinks. Use the mio_smc and
     * mio_smn convenience macros in mio_sink_multi.h to access this array.
     * Used by mio_sink_multi.h for output fanout support. Not valid on 
     * sources, for obvious reasons.
     */
    MIO_T_SINKARRAY = 8,
    /**
     * Source or sink pointer is a pointer to an SSL socket. Used for TLSv1
     * or DTLS via OpenSSL.
     */
    MIO_T_SOCK_TLS = 9,
} MIOType;

struct _MIOSource;
/**
 * An MIO data source. Represents a single, iterable, logical source of data, 
 * such as a directory of input files, or a passive socket. Create an MIOSource 
 * with one of the various mio_source_init_*() functions, or with
 * mio_config_source().
 */
typedef struct _MIOSource MIOSource;

struct _MIOSink;
/**
 * An MIO data sink. Represents a single, iterable, logical sink for data, 
 * such as a directory of output files, or an active socket. Create an MIOSink 
 * with one of the various mio_sink_init_*() functions, or with 
 * mio_config_sink().
 */
typedef struct _MIOSink MIOSink;

/**
 * An MIO source open/close function. Used internally by MIOSource.
 */

typedef gboolean        (*MIOSourceFn)(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err);

/**
 * An MIO source application-layer open/close function. Passed into
 * mio_dispatch() via an MIOAppDriver.
 */

typedef gboolean        (*MIOAppSourceFn)(
    MIOSource               *source,
    void                    *vctx,
    uint32_t                *flags,
    GError                  **err);

/**
 * An MIO source free function. Used internally by MIOSource.
 */

typedef void            (*MIOSourceFreeFn)(
    MIOSource               *source);

/**
 * An MIO sink open/close function. Used internally by MIOSink.
 */
typedef gboolean        (*MIOSinkFn)(
    MIOSource               *source,
    MIOSink                 *sink,
    uint32_t                *flags,
    GError                  **err);

/**
 * An MIO sink free function. Used internally by MIOSink.
 */

typedef void            (*MIOSinkFreeFn)(
    MIOSink                 *sink);
    
/**
 * An MIO sink application-layer open/close and record process function.
 * Passed into mio_dispatch() via an MIOAppDriver.
 */
 
typedef gboolean        (*MIOAppFn)(
    MIOSource               *source,
    MIOSink                 *sink,
    void                    *vctx,
    uint32_t                *flags,
    GError                  **err);

/** MIO data source structure */
struct _MIOSource {
    /** Source specifier */
    char                    *spec;
    /** Name of current source pointer */
    char                    *name;
    /** Type of current source pointer */
    MIOType                 vsp_type;
    /** Current source pointer */
    void                    *vsp;
    /** Source configuration; passed by caller to init. */
    void                    *cfg;
    /** Source context; privately managed by the source. */
    void                    *ctx;
    /** Next source function - called to iterate the source. */
    MIOSourceFn             next_source;
    /** Close source function - called to close the current source pointer. */
    MIOSourceFn             close_source;
    /** Free source function - called to free all source storage. */
    MIOSourceFreeFn         free_source;
    /** TRUE if next_source has been called; i.e., vsp is valid. */
    gboolean                opened;
    /** TRUE if application's source open function has been called. */
    gboolean                active;
};

/** MIO data sink structure. */
struct _MIOSink {
    /** Sink specifier */
    char                    *spec;
    /** Name of current sink pointer */
    char                    *name;
    /** Type of current sink pointer */
    MIOType                 vsp_type;
    /** Current sink pointer */
    void                    *vsp;
    /** Sink configuration; passed by caller to init. */
    void                    *cfg;
    /** Sink context; privately managed by the sink. */
    void                    *ctx;
    /** Next sink function - called to iterate the sink */
    MIOSinkFn               next_sink;
    /** Close sink function - called to close the current sink pointer. */
    MIOSinkFn               close_sink;
    /** Free sink function - called to free all sink storage. */
    MIOSinkFreeFn           free_sink;
    /** TRUE if next_sink has been called; i.e., vsp is valid. */
    gboolean                opened;
    /** TRUE if application's sink open function has been called. */
    gboolean                active;
    /** 
     * TRUE if next_sink will open a different current sink pointer 
     * each time; FALSE for single outputs like sockets, single files, 
     * and standard output.
     */
    gboolean                iterative;
};

/**
 * An MIO application driver. Applications should pass one of these to 
 * mio_dispatch(); the functions are then called in the appropriate order by 
 * the mio_dispatch() run loop.
 */
typedef struct _MIOAppDriver {
    /** 
     * Application source open function. Called after a new iteration of a 
     * source has been opened. This function should set up any internal state 
     * required to read records, read file or message headers, etc. 
     */
    MIOAppSourceFn          app_open_source;
    /** 
     * Application sink open function. Called after a new iteration of a sink 
     * has been opened. This function should set up any internal state required 
     * to write records, write file or message headers, etc. 
     */
    MIOAppFn                app_open_sink;
    /**
     * Application record processing function. Continually called by 
     * mio_dispatch(), this function should process a single input record,
     * then return. 
     */
    MIOAppFn                app_process;
    /**
     * Application source close function. Called after mio_dispatch() has 
     * determined that the source should be closed, but before closing it.
     * Use this to clean up after your app_open_source function.
     */
    MIOAppSourceFn          app_close_source;
    /**
     * Application sink close function. Called after mio_dispatch() has 
     * determined that the sink should be closed, but before closing it.
     * Use this to flush any pending application data to the sink, and to
     * clean up after your app_open_sink function.
     */
    MIOAppFn                app_close_sink;
} MIOAppDriver;

/** 
 * Convenience macro to get a source or sink's currently open
 * file descriptor. Valid for MIO_T_FD and MIO_T_SOCK_* types.
 */
#define mio_fd(_s_) GPOINTER_TO_INT((_s_)->vsp)

/** 
 * Convenience macro to get a source or sink's currently open
 * file pointer. Only valid if the source or sink's vsp_type is MIO_T_FP.
 */
#define mio_fp(_s_) ((FILE *)(_s_)->vsp)

/**
 * MIO primary dispatch function. Processes a single record. Ensures that 
 * source and sink are available before processing the record, and ensures 
 * that source and sink are properly torn down afterward as necessary.
 * Applications should probably use mio_dispatch_loop() instead, as it provides
 * for such niceties as polling and retry.
 *
 * @param source    An MIOSource to get data from. mio_dispatch() may cause
 *                  this source to close if it is open, and may cause this 
 *                  source to iterate if not.
 * @param sink      An MIOSink to send data to. mio_dispatch() may cause
 *                  this sink to close if it is open, and may cause this 
 *                  sink to iterate if not.
 * @param app_drv   An MIOAppDriver containing application functions to call
 *                  source and sink open and close, and for actual record 
 *                  processing.
 * @param vctx      Application-level context passed to each of the functions
 *                  in the MIOAppDriver.
 * @param flags     Pointer to MIO dispatch flags (a bitwise union of MIO_F_*
 *                  constants). These flags will be modified by mio_dispatch();
 *                  check the MIO_F_CTL_* flags after calling mio_dispatch to
 *                  determine whether an error was transient, whether there was
 *                  any input available, whether the application should 
 *                  terminate, and whether the source or sink were closed.
 * @param err       A GError error description (out)
 * @return          TRUE if a record was successfully processed, 
 *                  FALSE otherwise (setting error and flags).
 */
gboolean                mio_dispatch(
    MIOSource               *source,
    MIOSink                 *sink,
    MIOAppDriver            *app_drv,
    void                    *vctx,
    uint32_t                *flags,
    GError                  **err);
    
/**
 * MIO dispatch loop convenience function. Calls mio_dispatch() repeatedly, 
 * logging any errors that occur as warnings, until mio_dispatch sets 
 * MIO_F_CTL_TERMINATE to terminate the application. Automatically sleeps on
 * no input. Automatically backs off on transient errors.
 *
 * @param source    An MIOSource to get data from. mio_dispatch() may cause
 *                  this source to close if it is open, and may cause this 
 *                  source to iterate if not.
 * @param sink      An MIOSink to send data to. mio_dispatch() may cause
 *                  this sink to close if it is open, and may cause this 
 *                  sink to iterate if not.
 * @param app_drv   An MIOAppDriver containing application functions to call
 *                  source and sink open and close, and for actual record 
 *                  processing.
 * @param vctx      Application-level context passed to each of the functions
 *                  in the MIOAppDriver.
 * @param flags     Pointer to MIO dispatch flags (a bitwise union of MIO_F_*
 *                  constants). These flags will be modified by mio_dispatch().
 * @param polltime  time, in seconds, to delay on no input.
 * @param retrybase time, in seconds, to delay on the first transient error.
 *                  Each subsequent transient error doubles the delay up to
 *                  the maximum.
 * @param retrymax  
 
 */
 
gboolean                    mio_dispatch_loop(
    MIOSource               *source,
    MIOSink                 *sink,
    MIOAppDriver            *app_drv,
    void                    *vctx,
    uint32_t                flags,
    uint32_t                polltime,
        uint32_t                retrybase,
    uint32_t                retrymax);

/**
 * Frees an MIOSource by calling its source_free function.
 *
 * @param source MIOSource to free
 */

void mio_source_free(
    MIOSource               *source);

/**
 * Frees an MIOSink by calling its sink_free function.
 *
 * @param sink MIOSink to free
 */

void mio_sink_free(
    MIOSink                 *sink);

/**
 * Initialize an application-specific source. The MIO facility will not do any
 * low-level open or close; dispatch on this source will require the
 * application driver to handle all the details of source management.
 *
 * @param source    pointer to MIOSource to initialize. This MIOSource will 
 *                  be overwritten.
 * @param spec      input specifier to initialize MIOSource with.
 *                  The application driver will use this to do its own
 *                  low-level open.
 * @param vsp_type  requested source pointer type, or MIO_T_ANY for default.
 *                  Only the default, MIO_T_APP, is supported.
 * @param cfg       pointer to configuration context. 
 *                  The application driver will use this to do its own
 *                  low-level source configuration.
 * @param err       An error description pointer.
 * @return TRUE if the MIOSource was successfully initialized.
 */

gboolean mio_source_init_app(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err);

/**
 * Initialize an application-specific sink. The MIO facility will not do any
 * low-level open or close; dispatch on this source will require the
 * application driver to handle all the details of sink management.
 *
 * @param sink      pointer to MIOSink to initialize. This MIOSink will 
 *                  be overwritten.
 * @param spec      input specifier to initialize MIOSink with.
 *                  The application driver will use this to do its own
 *                  low-level open.
 * @param vsp_type  requested sink pointer type, or MIO_T_ANY for default.
 *                  Only the default, MIO_T_APP, is supported.
 * @param cfg       pointer to configuration context. 
 *                  The application driver will use this to do its own
 *                  low-level sink configuration.
 * @param err       An error description pointer.
 * @return TRUE if the MIOSink was successfully initialized.
 */

gboolean mio_sink_init_app(
    MIOSink         *sink,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err);

/* end idem */
#endif
