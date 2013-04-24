#ifndef _QOF_CONFIG_H_
#define _QOF_CONFIG_H_

#include <yaf/autoinc.h>
#include <yaf/yaftab.h>
#include <yaf/yafrag.h>
#include <yaf/decode.h>
#include <yaf/ring.h>

#include "qofifmap.h"

#include <airframe/airlock.h>

typedef struct qfConfig_st {
    /* Features enabled by template selection */
    gboolean    enable_biflow;  // RFC5103 biflow export
    gboolean    enable_ipv4;    // IPv4 address export (false = map v4 to v6)
    gboolean    enable_ipv6;    // IPv6 address export (false = drop v6)
    gboolean    enable_mac;     // store MAC addresses and vlan tags
    gboolean    enable_dyn;     // qfdyn master switch (seqno tracking)
    gboolean    enable_dyn_rtx; // qfdyn seqno rtx/loss detection
    gboolean    enable_dyn_rtt; // qfdyn RTT/IAT calculation
    gboolean    enable_tcpopt;  // require TCP options parsing
    gboolean    enable_iface;   // store interface information
    /* Features enabled explicitly */
    gboolean    enable_silk;    // SiLK compatibility mode
    gboolean    enable_gre;     // GRE decap mode
    /* Flow state configuration */
    uint32_t    ato_ms;
    uint32_t    ito_ms;
    uint32_t    max_flowtab;
    uint32_t    max_fragtab;
    uint64_t    max_flow_pkt;     // max packet count to force ATO (silk mode)
    uint64_t    max_flow_oct;     // max octet count to force ATO  (silk mode)
    uint32_t    ato_rtts;         // multiple of RTT to force ATO
    /* TCP dynamics state configuration */
    uint32_t    rtx_span;
    uint32_t    rtx_scale;
    /* Interface map */
    qfIfMap_t   ifmap;
} qfConfig_t;

struct qfTraceSource_t;

typedef struct qfInputContext_st {
    /** Input specifier */
    char            *inuri;
    /** BPF expression for packet filtering (PCAP/libtrace only) */
    char            *bpf_expr;
    /** Packet source */
    struct qfTraceSource_st *pktsrc;
    /** Keep going no matter what */
    gboolean        bulletproof;
} qfInputContext_t;

typedef struct qfOutputContext_st {
    /** Output specifier */
    char            *outspec;
    /** Output transport name */
    char            *transport;
    /** Use TLS */
    gboolean        enable_tls;
    /** Fixbuf (output) connection specifier */
    fbConnSpec_t    connspec;
    /** Observation domain ID */
    uint32_t        odid;
    /** UDP template retransmission (in ms) */
    uint32_t        template_rtx_period;
    /** Last UDP template retransmission */
    uint64_t        template_rtx_last;
    /** File rotation period (in ms) */
    uint32_t        rotate_period;
    /** Last file rotation */
    uint64_t        rotate_last;
    /** Statistics export period (in ms) (0 = disable) */
    uint32_t        stats_period;
    /** Last statistics export time */
    uint64_t        stats_last;
    /** Use output lock buffer */
    gboolean        enable_lock;
    /** Output lock buffer */
    AirLock         lockbuf;
    /** Output IPFIX buffer */
    fBuf_t          *fbuf;
} qfOutputContext_t;

typedef struct qfContext_st {
    /** Core configuration */
    qfConfig_t          cfg;
    /** Input configuration */
    qfInputContext_t    ictx;
    /** Output configuration */
    qfOutputContext_t   octx;
    /** Packet ring buffer */
    rgaRing_t           *pbufring;
    /** Decoder */
    yfDecodeCtx_t       *dectx;
    /** Flow table */
    yfFlowTab_t         *flowtab;
    /** Fragment table */
    yfFragTab_t         *fragtab;
    /** Error description */
    GError              *err;
} qfContext_t;

void qfConfigDefaults(qfConfig_t           *cfg);

gboolean qfConfigDotfile(qfConfig_t           *cfg,
                         const char           *filename,
                         GError               **err);

void qfContextSetup(qfContext_t           *ctx);

void qfContextTeardown(qfContext_t        *ctx);

void qfContextTerminate(qfContext_t *ctx);

#endif
