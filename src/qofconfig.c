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
    uint32_t    ato_rtt_multiple; // multiple of RTT to force ATO
    /* TCP dynamics state configuration */
    uint32_t    rtt_samples;
    uint32_t    rtx_distance;
    uint32_t    rtx_scale;
    /* Interface map */
    qfIfMap_t   ifmap;
} qfConfig_t;

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
    
    /* end of mapping or sequence */
    if ((event.type == YAML_MAPPING_END_EVENT) ||
        (event.type == YAML_SEQUENCE_END_EVENT))
    {
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
                                  uint32_t           *val,
                                  GError             **err)
{
    char valbuf[VALBUF_SIZE];
    
    if (!qfYamlParseValue(parser, valbuf, sizeof(valbuf), err)) return FALSE;

}

static gboolean qfYamlParseU64(yaml_parser_t      *parser,
                                  uint32_t           *val,
                                  GError             **err)
{
    
}

static gboolean qfYamlParsePrefixedV4(yaml_parser_t      *parser,
                                      uint32_t           *addr,
                                      unsigned int       *mask,
                                      GError             **err)
{
    
}

static gboolean qfYamlParsePrefixedV6(yaml_parser_t      *parser,
                                      uint32_t           *addr,
                                      unsigned int       *mask,
                                      GError             **err)
{
    
}


static gboolean qfYamlTemplate(qfConfig_t       *cfg,
                               yaml_parser_t    *parser,
                               GError           **err)
{

}

static gboolean qfYamlDocument(qfConfig_t       *cfg,
                               yaml_parser_t    *parser,
                               GError           **err)
{
    if (!qfYamlNextEvent(parser, err))
    switch ()
}

gboolean qfConfigParseYaml(qfConfig_t           *cfg,
                           const char           *filename,
                           GError               **err)
{
    
}
}
