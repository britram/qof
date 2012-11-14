/*
 ** mio_source_pcap.c
 ** Multiple I/O pcap source, from files, directories, or live capture
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
#include "mio_internal.h"
#include <airframe/mio_source_pcap.h>

#if HAVE_LIBPCAP

static char mio_pcap_errbuf[PCAP_ERRBUF_SIZE];

static gboolean mio_source_next_pcap_offline(
    MIOSourceFn             source_next_file,
    MIOSourceFn             source_close_file,
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    MIOSourcePCapFileConfig *cfg = (MIOSourcePCapFileConfig *)source->cfg;
    GError                  *cerr = NULL;
    gboolean                ok = TRUE;
    pcap_t                  *pcap = NULL;
    struct bpf_program      bpf;
    
    /* Fake vsp type to NULL */
    source->vsp_type = MIO_T_NULL;
    
    /* Get next filename into name */
    ok = source_next_file(source, flags, err);
    
    /* fail on file open error */
    if (!ok) goto end;
    
    /* Okay. We have a filename and we own it. Open it as a pcap context. */
    pcap = pcap_open_offline(source->name, mio_pcap_errbuf);
    
    /* check for pcap open error */
    if (!pcap) {
        ok = FALSE;
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_IO,
                    "couldn't open pcap file %s: %s",
                    source->name, mio_pcap_errbuf);
        goto errfile;
    }
  
    /* attach filter */
    if (cfg->filter) {
        if (pcap_compile(pcap, &bpf, cfg->filter, 1, 0) < 0) {
            ok = FALSE;
            g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                        "couldn't compile BPF expression %s: %s",
                        cfg->filter, pcap_geterr(pcap));
            goto errbpf;
        }
        pcap_setfilter(pcap, &bpf);
        pcap_freecode(&bpf);
    }
    
    /* stuff pcap context into vsp */
    source->vsp = pcap;
   
    goto end;

errbpf:
    /* close pcap context */
    pcap_close(pcap);

errfile:
    /* set fatal error flag */
    *flags |= MIO_F_CTL_ERROR;

    /* close file */
    if (!source_close_file(source, flags, &cerr)) {
        g_clear_error(err);
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_IO,
                    "couldn't close pcap file after pcap error %s: %s",
                    mio_pcap_errbuf, cerr->message);
        g_clear_error(&cerr);
    }

end:
    /* Restore vsp type */
    source->vsp_type = MIO_T_PCAP;
    
    return ok;
}


static gboolean mio_source_next_pcap_file_dir(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    return mio_source_next_pcap_offline(mio_source_next_file_dir, 
                                        mio_source_close_file,
                                        source, flags, err);
}

static gboolean mio_source_next_pcap_file_glob(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    return mio_source_next_pcap_offline(mio_source_next_file_glob, 
                                        mio_source_close_file,
                                        source, flags, err);
}

static gboolean mio_source_next_pcap_file_single(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    return mio_source_next_pcap_offline(mio_source_next_file_single, 
                                        mio_source_close_file,
                                        source, flags, err);
}

static gboolean mio_source_next_pcap_stdin(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    return mio_source_next_pcap_offline(mio_source_check_stdin, 
                                        mio_source_close_stdin,
                                        source, flags, err);
}

static gboolean mio_source_next_pcap_live(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    MIOSourcePCapLiveConfig *cfg = (MIOSourcePCapLiveConfig *)source->cfg;
    pcap_t                  *pcap = NULL;
    struct bpf_program      bpf;

    /* Go ahead and stash the name */
    source->name = source->spec;
    
    pcap = pcap_open_live(source->name, cfg->snaplen, 1, 
                          cfg->timeout, mio_pcap_errbuf);
    
    /* check for pcap open error */
    if (!pcap) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_IO,
                    "couldn't open pcap interface %s: %s",
                    source->name, mio_pcap_errbuf);
        goto errcap;
    }
    
    /* attach filter */
    if (cfg->filter) {
        if (pcap_compile(pcap, &bpf, cfg->filter, 1, 0) < 0) {
            g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                        "couldn't compile BPF expression %s: %s",
                        cfg->filter, pcap_geterr(pcap));
            goto errbpf;
        }
        pcap_setfilter(pcap, &bpf);
        pcap_freecode(&bpf);
    }
  
    /* stuff pcap context into vsp */
    source->vsp = pcap;
    
    return TRUE;
    
errbpf:
    /* close context */
    pcap_close(pcap);
    
errcap:
    /* bug out. die if we can't open the interface. */
    *flags |= (MIO_F_CTL_ERROR | MIO_F_CTL_TERMINATE);
    return FALSE;
}

static gboolean mio_source_close_pcap_offline(
    MIOSourceFn             source_close_file,
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    gboolean                ok;
    
    /* close down pcap context */
    pcap_close((pcap_t *)source->vsp);
    
    /* Fake vsp type to NULL */
    source->vsp_type = MIO_T_NULL;
    
    /* Close file */
    ok = source_close_file(source, flags, err);
    
    /* Restore vsp type */
    source->vsp_type = MIO_T_PCAP;
    
    return ok;
}

static gboolean mio_source_close_pcap_file(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    return mio_source_close_pcap_offline( 
        mio_source_close_file, source, flags, err);
}

static gboolean mio_source_close_pcap_stdin(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    return mio_source_close_pcap_offline(
        mio_source_close_stdin, source, flags, err);
}

static gboolean mio_source_close_pcap_live(
    MIOSource               *source,
    uint32_t                *flags,
    GError                  **err)
{
    /* close down pcap context */
    pcap_close((pcap_t *)source->vsp);

    return TRUE;
}

static gboolean mio_source_init_pcap_inner(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    MIOSourceFn     next_source,
    MIOSourceFn     close_source,
    MIOSourceFreeFn free_source,
    GError          **err)
{
    /* choose default type */
    if (vsp_type == MIO_T_ANY) vsp_type = MIO_T_PCAP;

    /* initialize file source */
    source->spec = g_strdup(spec);
    source->name = NULL;
    source->vsp_type = vsp_type;
    source->vsp = NULL;
    source->ctx = NULL;
    source->cfg = cfg;
    source->next_source = next_source;
    source->close_source = close_source;
    source->free_source = free_source;
    source->opened = FALSE;
    source->active = FALSE;
    
    /* Ensure type is valid */
    if (vsp_type != MIO_T_PCAP) {
        g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_ARGUMENT,
                    "Cannot create pcap source: type mismatch");
        return FALSE;        
    }
    
    return TRUE;
}

gboolean mio_source_init_pcap_dir(
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
    return mio_source_init_pcap_inner(source, spec, vsp_type, cfg, 
                                      mio_source_next_pcap_file_dir,
                                      mio_source_close_pcap_file,
                                      mio_source_free_file, err);
}

gboolean mio_source_init_pcap_glob(
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
        return mio_source_init_pcap_single(source, spec, vsp_type, cfg, err);
    }

    /* initialize source */
    return mio_source_init_pcap_inner(source, spec, vsp_type, cfg, 
                                      mio_source_next_pcap_file_glob,
                                      mio_source_close_pcap_file,
                                      mio_source_free_file, err);
}

gboolean mio_source_init_pcap_single(
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
        return mio_source_init_pcap_stdin(source, spec, vsp_type, cfg, err);
    }    

    /* initialize source */
    return mio_source_init_pcap_inner(source, spec, vsp_type, cfg, 
                                      mio_source_next_pcap_file_single,
                                      mio_source_close_pcap_file,
                                      mio_source_free_file, err);
}

gboolean mio_source_init_pcap_stdin(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{
    gboolean        ok = TRUE;
    
    /* initialize source */
    ok = mio_source_init_pcap_inner(source, spec, vsp_type, cfg, 
                                    mio_source_next_pcap_stdin,
                                    mio_source_close_pcap_stdin,
                                    mio_source_free_file, err);
    if (!ok) return ok;
    
    /* reflect the fact that stdin is already open */
    source->name = "-";
    
    return TRUE;
}

gboolean mio_source_init_pcap_live(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{    
    /* initialize source */
    return mio_source_init_pcap_inner(source, spec, vsp_type, cfg, 
                                      mio_source_next_pcap_live,
                                      mio_source_close_pcap_live,
                                      NULL, err);    
}

#else /* if HAVE_LIBCAP */
gboolean mio_source_init_pcap_dir(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{
    g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_IMPL,
                "libairframe was built without libpcap support");
    return FALSE;
}

gboolean mio_source_init_pcap_glob(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{
    g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_IMPL,
                "libairframe was built without libpcap support");
    return FALSE;
}

gboolean mio_source_init_pcap_single(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{
    g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_IMPL,
                "libairframe was built without libpcap support");
    return FALSE;
}

gboolean mio_source_init_pcap_stdin(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{
    g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_IMPL,
                "libairframe was built without libpcap support");
    return FALSE;
}

gboolean mio_source_init_pcap_live(
    MIOSource       *source,
    const char      *spec,
    MIOType         vsp_type,
    void            *cfg,
    GError          **err)
{
    g_set_error(err, MIO_ERROR_DOMAIN, MIO_ERROR_IMPL,
                "libairframe was built without libpcap support");
    return FALSE;
}
#endif

