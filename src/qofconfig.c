/**
 ** qofconfig.c
 ** QoF configuration -- parse YAML configuration into QOF config object
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2013      Brian Trammell. All Rights Reserved
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell <brian@trammell.ch>
 ** ------------------------------------------------------------------------
 ** QoF is made available under the terms of the GNU Public License (GPL)
 ** Version 2, June 1991
 ** ------------------------------------------------------------------------
 */

#define _YAF_SOURCE_
#include <yaf/autoinc.h>
#include "qofifmap.h"

#include <arpa/inet.h>

#include <yaml.h>

#define VALBUF_SIZE     80
#define ADDRBUF_SIZE    42

typedef qfConfig_st {
    /* Template */
    // FIXME how to specify template?
    /* Features enabled by template selection */
    gboolean    enable_biflow;  // RFC5103 biflow export 
    gboolean    enable_ipv4     // IPv4 address export (false = map v4 to v6)
    gboolean    enable_ipv6     // IPv6 address export (false = drop v6)
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

typedef enum {
    QF_CONFIG_NOTYPE = 0,
    QF_CONFIG_STRING,
    QF_CONFIG_U32,
    QF_CONFIG_U64,
    QF_CONFIG_BOOL,
} qfConfigKeyType_t;

typedef struct qfConfigKeyAction_st {
    char                *key;
    size_t              off;
    qfConfigKeyType_t   vt;
} qfConfigKeyAction_t;

#define CFG_OFF(_F_) offsetof(qfConfig_t, _F_)

static qfConfigKeyAction_t cfg_key_actions[] = {
    {"active-timeout",         CFG_OFF(ato_ms), QF_CONFIG_U32},
    {"idle-timeout",           CFG_OFF(ito_ms), QF_CONFIG_U32},
    {"max-flows",              CFG_OFF(max_flowtab), QF_CONFIG_U32},
    {"max-frags",              CFG_OFF(max_fragtab), QF_CONFIG_U32},
    {"active-timeout-octets",  CFG_OFF(max_flow_oct), QF_CONFIG_U64},
    {"active-timeout-packets", CFG_OFF(max_flow_pkt), QF_CONFIG_U64},
    {"active-timeout-rtts",    CFG_OFF(ato_rtts), QF_CONFIG_U32},
    {"rtx-span",               CFG_OFF(rtx_span), QF_CONFIG_U32},
    {"rtx-scale",              CFG_OFF(rtx_scale), QF_CONFIG_U32},
    {"rtt-samples",            CFG_OFF(rtt_samples), QF_CONFIG_U32},
    {"gre-decap",              CFG_OFF(enable_gre), QF_CONFIG_BOOL},
    {"silk-compatible",        CFG_OFF(enable_silk), QF_CONFIG_BOOL},
    {NULL, NULL, QF_CONFIG_NOTYPE}
}

static qfConfigKeyAction_t cfg_ie_features[] = {
    {"sourceIPv4Address",       CFG_OFF(enable_ipv4), QF_CONFIG_BOOL},
    {"destinationIPv4Address",  CFG_OFF(enable_ipv4), QF_CONFIG_BOOL},
    {"sourceIPv6Address",       CFG_OFF(enable_ipv6), QF_CONFIG_BOOL},
    {"destinationIPv6Address",  CFG_OFF(enable_ipv6), QF_CONFIG_BOOL},
    {"sourceMacAddress",        CFG_OFF(enable_mac), QF_CONFIG_BOOL},
    {"destinationMacAddress",   CFG_OFF(enable_mac), QF_CONFIG_BOOL},
    {"vlanId",                  CFG_OFF(enable_mac), QF_CONFIG_BOOL},
    {"tcpSequenceCount",        CFG_OFF(enable_dyn), QF_CONFIG_BOOL},
    {"maxTcpReorderSize",       CFG_OFF(enable_dyn), QF_CONFIG_BOOL},
    {"tcpSequenceLossCount",    CFG_OFF(enable_dyn_rtx), QF_CONFIG_BOOL},
    {"tcpRetransmitCount",      CFG_OFF(enable_dyn_rtx), QF_CONFIG_BOOL},
    {"minTcpRttMilliseconds",   CFG_OFF(enable_dyn_rtt), QF_CONFIG_BOOL},
    {"meanTcpRttMilliseconds",  CFG_OFF(enable_dyn_rtt), QF_CONFIG_BOOL},
    {"maxTcpFlightSize",        CFG_OFF(enable_dyn_rtt), QF_CONFIG_BOOL},
    {NULL, NULL, QF_CONFIG_NOTYPE}
}

static gboolean qfYamlError(GError                  **err,
                            const yaml_parser_t     *parser,
                            const char              *errmsg)
{
    if (!filename) {
        filename = "<no file given>";
    }
    
    if (errmsg) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                    "QoF config error: %s at line %zd, column %zd\n",
                    errmsg, parser->mark.line, parser->mark.column);
    } else {
        switch (parser->error) {
            case YAML_MEMORY_ERROR:
                g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                            "YAML parser out of memory.\n");
                break;
            case YAML_READER_ERROR:
                if (parser->problem_value != -1) {
                    g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                                "YAML reader error: %s: #%X at %zd\n",
                                parser->problem, parser->problem_value,
                                parser->problem_offset);
                } else {
                    g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                                "YAML reader error: %s at %zd\n",
                                parser->problem, parser->problem_offset);
                }
                break;
            case YAML_SCANNER_ERROR:
            case YAML_PARSER_ERROR:
                if (parser->context) {
                    g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                                "YAML parser error: %s at line %ld, column %ld\n"
                                "%s at line %ld, column %ld\n",
                                parser->context, parser->context_mark.line+1,
                                parser->context_mark.column+1,
                                parser->problem, parser->problem_mark.line+1,
                                parser->problem_mark.column+1);
                } else {
                    g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                                "YAML parser error: %s at line %ld, column %ld\n",
                                parser->problem, parser->problem_mark.line+1,
                                parser->problem_mark.column+1);
                }
                break;
            default:
                g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                            "YAML parser internal error.\n");
        }
    }
    return FALSE;
 }


static gboolean qfYamlParseKey(yaml_parser_t      *parser,
                               char               *keybuf,
                               size_t             keybuf_sz,
                               GError             **err)

    yaml_event_t    event;

    /* get event */
    if (!yaml_parser_parse(parser, &event)) {
        return qfYamlError(err, parser, NULL);
    }

    /* end of mapping */
    if ((event.type == YAML_MAPPING_END_EVENT)) return FALSE;

    /* not scalar */
    if ((event.type != YAML_SCALAR_EVENT)) {
        return qfYamlError(err, parser, "expected key");
    }

    /* get string */
    strncpy(keybuf, (const char*)event.data.scalar.value, keybuf_sz);
    return TRUE;
}


static gboolean qfYamlParseValue(yaml_parser_t      *parser,
                                 char               *valbuf,
                                 size_t             valbuf_sz,
                                 GError             **err)
{
    
    yaml_event_t    event;
    
    /* get event */
    if (!yaml_parser_parse(parser, &event)) {
        return qfYamlError(err, parser, NULL);
    }
    
    /* end of sequence is okay */
    if (event.type == YAML_SEQUENCE_END_EVENT) {
        return FALSE;
    }
    
    /* not scalar */
    if ((event.type != YAML_SCALAR_EVENT)) {
        return qfYamlError(err, parser, "expected scalar value");
    }
    
    /* get string */
    strncpy(keybuf, (const char*)event.data.scalar.value, keybuf_sz);
    return TRUE;
}

static gboolean qfYamlParseU32(yaml_parser_t      *parser,
                                  uint32_t        *val,
                                  GError          **err)
{
    char valbuf[VALBUF_SIZE];
    int rv;
    
    if (!qfYamlParseValue(parser, valbuf, sizeof(valbuf), err)) return FALSE;

    if (sscanf(valbuf, "%u", val) < 1) {
        return qfYamlError(err, parser, "expected integer value");
    }
    
    return TRUE;
}


static gboolean qfYamlParseU64(yaml_parser_t      *parser,
                               uint64_t           *val,
                               GError             **err)
{
    char valbuf[VALBUF_SIZE];
    int rv;
    
    if (!qfYamlParseValue(parser, valbuf, sizeof(valbuf), err)) return FALSE;
    
    if (sscanf(valbuf, "%llu", val) < 1) {
        return qfYamlError(err, parser, "expected integer value");
    }
    
    return TRUE;
}

static gboolean qfYamlParsePrefixedV4(yaml_parser_t      *parser,
                                      uint32_t           *addr,
                                      uint8_t            *mask,
                                      GError             **err)
{
    char valbuf[VALBUF_SIZE];
    char addrbuf[ADDRBUF_SIZE];
    int rv;
    
    if (!qfYamlParseValue(parser, valbuf, sizeof(valbuf), err)) return FALSE;
    
    rv = sscanf(valbuf, "%15[0-9.]/%hhu", addrbuf, mask));
    if (rv < 1) {
        return qfYamlError(err, parser, "expected IPv4 address");        
    } else if (rv == 1) {
        *mask = 32; // implicit mask
    }
    
    rv = inet_pton(AF_INET, addrbuf, addr);
    if (rv == 0) {
       return qfYamlError(err, parser, "invalid IPv4 address");
    } else if (rv == -1) {
       return qfYamlError(err, parser, strerror(errno));
    }

    return TRUE;
}

static gboolean qfYamlParsePrefixedV6(yaml_parser_t      *parser,
                                      uint32_t           *addr,
                                      uint8_t            *mask,
                                      GError             **err)
{
    char valbuf[VALBUF_SIZE];
    char addrbuf[ADDRBUF_SIZE];
    int rv;
    
    if (!qfYamlParseValue(parser, valbuf, sizeof(valbuf), err)) return FALSE;
    
    rv = sscanf(valbuf, "%39[0-9a-fA-F:.]/%hhu", addrbuf, mask));
    if (rv < 1) {
        return qfYamlError(err, parser, "expected IPv6 address");
    } else if (rv == 1) {
        *mask = 128; // implicit mask
    }
    
    rv = inet_pton(AF_INET, addrbuf, addr);
    if (rv == 0) {
        return qfYamlError(err, parser, "invalid IPv6 address");
    } else if (rv == -1) {
        return qfYamlError(err, parser, strerror(errno));
    }
    
    return TRUE;
}

static gboolean qfYamlRequireEvent(yaml_parser_t        *parser,
                                   yaml_event_type_t    et,
                                   GError               **err)
{
    yaml_event_t event;

    /* get event */
    if (!yaml_parser_parse(parser, &event)) {
        return qfYamlError(err, parser, NULL);
    }

    /* check against expected */
    if (event_type != et) {
        return qfYamlError(err, parser, "malformed configuration")
    }

    return TRUE;
}

static gboolean qfYamlTemplate(qfConfig_t       *cfg,
                               yaml_parser_t    *parser,
                               GError           **err)
{
    char valbuf[VALBUF_SIZE];
    
    /* consume sequence start for template list */
    if (!qfYamlRequireEvent(parser, YAML_SEQUENCE_START_EVENT)) return FALSE;

    while (qfYamlParseValue(parser, valbuf, sizeof(valbuf), err)) {
        
    }

    /* check for error on last value parse */
    if (*err) return FALSE;

    /* done */
    return TRUE;
}

static gboolean qfYamlParseMapEntry(yaml_parser_t *parser,
                                    uint32_t        *v4addr,
                                    uint8_t         *v4mask,
                                    uint8_t         *v6addr,
                                    uint8_t         *v6mask,
                                    uint8_t         *ingress,
                                    uint8_t         *egress)
{
    // FIXME code goes here
}

static gboolean qfYamlInterfaceMap(qfConfig_t       *cfg,
                                   yaml_parser_t    *parser,
                                   GError           **err)
{
    uint32_t    v4addr = 0;
    uint8_t     v6addr[16];
    
    /* consume sequence start for interface-map mapping list */
    if (!qfYamlRequireEvent(parser, YAML_SEQUENCE_START_EVENT)) return FALSE;
    
    memset(v6addr, 0, sizeof(v6addr));
    
    /* consume map entries until we see a sequence end */
    while (qfYamlParseMapEntry(parser, &v4addr, &v4mask,
                               &v6addr, &v6mask, &ingress, &egress, err))
    {
        if (v4addr) {
            qfIfMapAddIPv4Mapping(&cfg->map, v4addr, v4mask, ingress, egress);
            v4addr = 0;
        }
        
        if (v6addr[0]) {
            qfIfMapAddIPv6Mapping(&cfg->map, v6addr, v6mask, ingress, egress);
            memset(v6addr, 0, sizeof(v6addr));
        }
    }
    
    /* check for error on last entry parse */
    if (*err) return FALSE;
    
    /* done */
    return TRUE;
}


static gboolean qfYamlDocument(qfConfig_t       *cfg,
                               yaml_parser_t    *parser,
                               GError           **err)
{
    
    int i, keyfound;
    gboolean rv;
    void *cvp;
    
    char keybuf[VALBUF_SIZE];
    
    /* consume events to get us into the main document map */
    if (!qfYamlRequireEvent(parser, YAML_STREAM_START_EVENT)) return FALSE;
    if (!qfYamlRequireEvent(parser, YAML_DOCUMENT_START_EVENT)) return FALSE;
    if (!qfYamlRequireEvent(parser, YAML_MAPPING_START_EVENT)) return FALSE;
    
    /* eat keys and dispatch actions */
    while (qfYamlParseKey(parser, keybuf, sizeof(keybuf), err)) {
        
        keyfound = 0;
        
        /* check for template */
        if (strncmp("template", keybuf, sizeof(keybuf) {
            if (!qfYamlTemplate(cfg, parser, err)) return NULL;
            keyfound = 1;
        }
        
        /* check interface map */
        if (strncmp("interface-map", keybuf, sizeof(keybuf) {
            if (!qfYamlInterfaceMap(cfg, parser, err)) return NULL;
            keyfound = 1;
        }
        
        /* nope; check action list */
        for (i = 0; (!keyfound) && cfg_key_actions[i].key; i++) {
            if (strncmp(keybuf, cfg_key_actions[i].key, sizeof(keybuf)) == 0) {
                
                /* get pointer into cfg, parse based on type */
                cvp = ((void *)cfg) + cfg_key_actions[i].off;
                switch (actions[i].vp) {
                  case QF_CONFIG_U32:
                    rv = qfYamlParseU32(parser, (uint32_t*)cvp, err);
                    break;
                  case QF_CONFIG_U64:
                    rv = qfYamlParseU64(parser, (uint64_t*)cvp, err);
                    break;
                  case QF_CONFIG_BOOL:
                    rv = qfYamlParseBool(parser, (gboolean*)cvp, err);
                    break;
                  default:
                    return qfYamlError(err, parser, "internal error in map dispatch");
                }

                /* check for error */
                if (!rv) return FALSE;
                
                /* good value, jump to next key */
                keyfound = 1;
            }
        }
        
        /* end of for loop, unknown key */
        return qfYamlError(err, parser, "unknown configuration parameter");
    }
    
    /* check for error on last key parse */
    if (*err) return FALSE;

    /* consume events to get us back out of the stream
       (note the end of the ParseKey loop consumed the mapping end) */
    if (!qfYamlRequireEvent(parser, YAML_DOCUMENT_END_EVENT)) return FALSE;
    if (!qfYamlRequireEvent(parser, YAML_STREAM_END_EVENT)) return FALSE;

    return TRUE;
}

gboolean qfConfigParseYaml(qfConfig_t           *cfg,
                           const char           *filename,
                           GError               **err)
{
    
}
