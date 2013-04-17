// FIXME replace qofctx with this interface, integrate throughout.

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
    uint32_t    rtt_samples;
    uint32_t    rtx_span;
    uint32_t    rtx_scale;
    /* Interface map */
    qfIfMap_t   ifmap;
} qfConfig_t;

typedef struct qfInputContext_st {
    /** Input specifier */
    char            *inspec;
    /** Packet input driver name */
    char            *livetype;
    /** BPF expression for packet filtering (PCAP/libtrace only) */
    char            *bpf_expr;
    /** Skip single input errors */
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
    /** UDP template retransmission (in ms) */
    uint32_t        rotate_period;
    /** Last UDP template retransmission */
    uint64_t        rotate_last;
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
    /** Packet source */
    void                *pktsrc;
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

gboolean qfConfigParseYaml(qfConfig_t           *cfg,
                           const char           *filename,
                           GError               **err);

void qfContextSetupOutput(qfOutputContext_t *octx);