/**
 ** qofctx.c
 ** QoF configuration
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
#include <yaf/qofifmap.h>

#include <arpa/inet.h>

#include <yaml.h>

#include "qofctx.h"

static gboolean qfYamlError(GError                  **err,
                        const yaml_parser_t     *parser,
                        const char              *filename,
                        const char              *errmsg)
{    
    if (!filename) {
        filename = "<no file given>";
    }
    
    if (errmsg) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                    "QoF config error: %s at line %zd, column %zd in file %s\n",
                    errmsg, parser->mark.line, parser->mark.column, filename);
    } else {
        switch (parser->error) {
            case YAML_MEMORY_ERROR:
                g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                            "YAML parser out of memory in file %s.\n", filename);
                break;
            case YAML_READER_ERROR:
                if (parser->problem_value != -1) {
                    g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                                "YAML reader error: %s: #%X at %zd in file %s\n",
                                parser->problem, parser->problem_value,
                                parser->problem_offset, filename);
                } else {
                    g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                                "YAML reader error: %s at %zd in file %s\n",
                                parser->problem, parser->problem_offset, filename);
                }
                break;
            case YAML_SCANNER_ERROR:
            case YAML_PARSER_ERROR:
                if (parser->context) {
                    g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                                "YAML parser error: %s at line %ld, column %ld\n"
                                "%s at line %ld, column %ld in file %s\n",
                                parser->context, parser->context_mark.line+1,
                                parser->context_mark.column+1,
                                parser->problem, parser->problem_mark.line+1,
                                parser->problem_mark.column+1,
                                filename);
                } else {
                    g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                                "YAML parser error: %s at line %ld, "
                                "column %ld in file %s\n",
                                parser->problem, parser->problem_mark.line+1,
                                parser->problem_mark.column+1,
                                filename);
                }
                break;
            default:
                g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                            "YAML parser internal error in file %s.\n",
                            filename);
        }
    }
    
    return FALSE;
}

/* States for the YAF configuration parser */
typedef enum {
    QCP_INIT,               // initial state
    QCP_IN_STREAM,          // in stream
    QCP_IN_DOC,             // in document
    QCP_IN_DOC_MAP,         // in mapping in document
    QCP_IN_IFMAP,           // expecting value (sequence) for ifmap
    QCP_IN_IFMAP_SEQ,       // in value (sequence) for ifmap
    QCP_IN_IFMAP_KEY,       // expecting key for ifmap
    QCP_IN_IFMAP_V4NET,     // expecting v4 address value for ifmap
    QCP_IN_IFMAP_V6NET,     // expecting v6 address value for ifmap
    QCP_IN_IFMAP_INGRESS,   // expecting ingress interface number for ifmap
    QCP_IN_IFMAP_EGRESS,    // expecting egress interface number for ifmap
    QCP_IN_ANONMODE,        // expecting value (boolean) for anon-mode
    QCP_IN_TMPL,            // expecting value (map) for template
    QCP_IN_TMPL_SEQ,        // expecting IE name (template sequence value)
} qcp_state_t;

#define QCP_SV (const char*)event.data.scalar.value

#define QCP_REQUIRE_SCALAR(_e_) \
    if (event.type != YAML_SCALAR_EVENT) { \
        return qfYamlError(err, &parser, filename, (_e_)); \
    }

#define QCP_SCALAR_NEXT_STATE(_v_, _s_) \
    if (strcmp((const char*)event.data.scalar.value, (_v_)) == 0) { \
        qcpstate = (_s_); \
        break; \
    }

#define QCP_EVENT_NEXT_STATE(_v_, _s_) \
    if (event.type == (_v_)) { \
        qcpstate = (_s_); \
        break; \
    }

#define QCP_DEFAULT(_e_) \
    return qfYamlError(err, &parser, filename, (_e_));

gboolean qfParseYamlConfig(yfContext_t           *ctx,
                           const char            *filename,
                           GError                **err)
{
    FILE            *cfgfile;
    yaml_parser_t   parser;
    yaml_event_t    event;
    qcp_state_t     qcpstate = QCP_INIT;
    
    char            addr4buf[16];
    uint32_t        addr4;
    unsigned int    pfx4;
    
    char            addr6buf[40];
    uint8_t         addr6[16];
    unsigned int    pfx6;
    
    unsigned int    ingress;
    unsigned int    egress;
    
    int             rv;
    
    // short circuit on no file
    if (!filename) return TRUE;
    
    // open file and initialize parser
    if (!(cfgfile = fopen(filename,"r"))) {
        g_set_error(err, YAF_ERROR_DOMAIN, YAF_ERROR_ARGUMENT,
                    "cannot open YAML configuration file %s: %s",
                    filename, strerror(errno));
        return FALSE;
    }
    
    memset(&parser, 0, sizeof(parser));
    if (!yaml_parser_initialize(&parser)) {
        return qfYamlError(err, &parser, NULL, NULL);
    }
    yaml_parser_set_input_file(&parser, cfgfile);

    while (1) {
        // get next event
        if (!yaml_parser_parse(&parser, &event)) {
            return qfYamlError(err, &parser, NULL, NULL);
        }
        
        // switch based on state
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        } else if (event.type != YAML_NO_EVENT) switch (qcpstate) {
                
            case QCP_INIT:               // initial state
                QCP_EVENT_NEXT_STATE(YAML_STREAM_START_EVENT, QCP_IN_STREAM)
                QCP_DEFAULT("internal error: missing stream start")
                break;
                
            case QCP_IN_STREAM:          // in stream
                QCP_EVENT_NEXT_STATE(YAML_DOCUMENT_START_EVENT, QCP_IN_DOC)
                QCP_EVENT_NEXT_STATE(YAML_STREAM_END_EVENT, QCP_INIT)
                QCP_DEFAULT("internal error: missing doc start")
                break;
                
            case QCP_IN_DOC:             // in document
                QCP_EVENT_NEXT_STATE(YAML_MAPPING_START_EVENT, QCP_IN_DOC_MAP)
                QCP_EVENT_NEXT_STATE(YAML_DOCUMENT_END_EVENT, QCP_IN_STREAM)
                QCP_DEFAULT("QoF config must consist of a single map")
                break;
                
            case QCP_IN_DOC_MAP:         // in mapping in document
                QCP_EVENT_NEXT_STATE(YAML_MAPPING_END_EVENT, QCP_IN_DOC)
                QCP_REQUIRE_SCALAR("internal error: missing key")
                QCP_SCALAR_NEXT_STATE("interface-map", QCP_IN_IFMAP)
                QCP_SCALAR_NEXT_STATE("anon-mode", QCP_IN_ANONMODE)
                QCP_SCALAR_NEXT_STATE("template", QCP_IN_TMPL)
                QCP_DEFAULT("unknown configuration key")
                break;
                
            case QCP_IN_IFMAP:           // expecting value (sequence) for interface-map
                QCP_EVENT_NEXT_STATE(YAML_SEQUENCE_START_EVENT, QCP_IN_IFMAP_SEQ)
                QCP_DEFAULT("missing sequence value for interface-map")
                break;
                
            case QCP_IN_IFMAP_SEQ:       // in value (sequence) for interface-map
                if (event.type == YAML_MAPPING_START_EVENT) {
                    // new mapping, clear buffers
                    addr4buf[0] = '\0';
                    addr4 = 0;
                    pfx4 = 0;
                    addr6buf[0] = '\0';
                    memset(addr6, 0, sizeof(addr6));
                    pfx6 = 0;
                    ingress = 0;
                    egress = 0;
                    qcpstate = QCP_IN_IFMAP_KEY;
                    break;
                }
                QCP_EVENT_NEXT_STATE(YAML_SEQUENCE_END_EVENT, QCP_IN_DOC_MAP)
                QCP_DEFAULT("missing mapping in interface-map")
                break;
                
            case QCP_IN_IFMAP_KEY:       // expecting key for interface-map
                if (event.type == YAML_MAPPING_END_EVENT) {
                    // end mapping, add buffers to map
                    if (addr4buf[0]) {
                        qfIfMapAddIPv4Mapping(&(ctx->ifmap), addr4, pfx4,
                                              ingress, egress);
                    }
                    if (addr6buf[0]) {
                        qfIfMapAddIPv6Mapping(&(ctx->ifmap), addr6, pfx6,
                                              ingress, egress);
                    }
                    qcpstate = QCP_IN_IFMAP_SEQ;
                    break;
                }
                
                QCP_REQUIRE_SCALAR("internal error: missing key")
                QCP_SCALAR_NEXT_STATE("ip4-net", QCP_IN_IFMAP_V4NET)
                QCP_SCALAR_NEXT_STATE("ip6-net", QCP_IN_IFMAP_V6NET)
                QCP_SCALAR_NEXT_STATE("ingress", QCP_IN_IFMAP_INGRESS)
                QCP_SCALAR_NEXT_STATE("egress", QCP_IN_IFMAP_EGRESS)
                QCP_DEFAULT("unknown interface-map key")
                break;
                
            case QCP_IN_IFMAP_V4NET:    // expecting v4 address value for ifmap
                if (event.type == YAML_SCALAR_EVENT) {
                    rv = sscanf(QCP_SV, "%15[0-9.]/%u", addr4buf, &pfx4);
                    if (rv == 1) {
                        pfx4 = 32; // implicit single host
                    } else if (rv != 2) {
                        return qfYamlError(err, &parser, filename,
                                           "invalid IPv4 network");
                    }
                    
                    if (pfx4 > 32) {
                        return qfYamlError(err, &parser, filename,
                                           "invalid IPv4 prefix length");
                    }
                    
                    fprintf(stderr, "ip4-net: %s/%u\n", addr4buf, pfx4);
                    
                    rv = inet_pton(AF_INET, addr4buf, &addr4);
                    if (rv == 0) {
                        return qfYamlError(err, &parser, filename,
                                           "invalid IPv4 address");
                    } else if (rv == -1) {
                        return qfYamlError(err, &parser, filename,
                                           strerror(errno));
                    }
                    
                    addr4 = ntohl(addr4);
                    
                    addr4 = addr4;
                    
                } else {
                    return qfYamlError(err, &parser, filename,
                                       "missing value for ip4-net");
                }
                qcpstate = QCP_IN_IFMAP_KEY;
                break;
                
            case QCP_IN_IFMAP_V6NET:    // expecting v6 address value for ifmap
                if (event.type == YAML_SCALAR_EVENT) {
                    rv = sscanf(QCP_SV, "%39[0-9a-fA-F:.]/%u", addr6buf, &pfx6);
                    if (rv == 1) {
                        pfx6 = 128; // implicit single host
                    } else if (rv != 2) {
                        return qfYamlError(err, &parser, filename,
                                           "invalid IPv6 network");
                    }
                    
                    if (pfx6 > 128) {
                        return qfYamlError(err, &parser, filename,
                                           "invalid IPv6 prefix length");
                    }
                    
                    fprintf(stderr, "ip6-net: %s/%u\n", addr6buf, pfx6);
                    
                    rv = inet_pton(AF_INET6, addr6buf, &addr6);
                    if (rv == 0) {
                        return qfYamlError(err, &parser, filename,
                                           "invalid IPv6 address");
                    } else if (rv == -1) {
                        return qfYamlError(err, &parser, filename,
                                           strerror(errno));
                    }
                    
                } else {
                    return qfYamlError(err, &parser, filename,
                                       "missing value for ip6-net");
                }
                qcpstate = QCP_IN_IFMAP_KEY;
                break;
                
            case QCP_IN_IFMAP_INGRESS:   // expecting ingress interface number for ifmap
                if (event.type == YAML_SCALAR_EVENT) {
                    rv = sscanf(QCP_SV, "%u", &ingress);
                    if (rv != 1 || ingress > 255) {
                        return qfYamlError(err, &parser, filename,
                                           "invalid ingress interface number");
                        return FALSE;
                    }
                } else {
                    return qfYamlError(err, &parser, filename,
                                       "missing value for ingress-if");
                }
                qcpstate = QCP_IN_IFMAP_KEY;
                break;
                
            case QCP_IN_IFMAP_EGRESS:    // expecting egress interface number for ifmap
                if (event.type == YAML_SCALAR_EVENT) {
                    rv = sscanf(QCP_SV, "%u", &egress);
                    if (rv != 1 || egress > 255) {
                        return qfYamlError(err, &parser, filename,
                                           "invalid egress interface number");
                    }
                } else {
                    return qfYamlError(err, &parser, filename,
                                       "missing value for egress-if");
                }
                qcpstate = QCP_IN_IFMAP_KEY;
                break;
            
            // FIXME this goes away when templates are runtime-specified
            case QCP_IN_ANONMODE:
                if (event.type == YAML_SCALAR_EVENT) {
                    if ((QCP_SV)[0] != '0') {
                        yfWriterExportAnon(TRUE);
                    }
                } else {
                    return qfYamlError(err, &parser, filename,
                                       "missing value for anon-mode");
                }
                qcpstate = QCP_IN_DOC_MAP;
                break;
            
            case QCP_IN_TMPL:
                QCP_EVENT_NEXT_STATE(YAML_SEQUENCE_START_EVENT, QCP_IN_TMPL_SEQ)
                QCP_DEFAULT("template must be a sequence of IE names")
                break;
                
            case QCP_IN_TMPL_SEQ:
                QCP_EVENT_NEXT_STATE(YAML_SEQUENCE_END_EVENT, QCP_IN_DOC_MAP)
                QCP_REQUIRE_SCALAR("template must be a sequence of IE names")
                
                if (!yfWriterSpecifyExportIE(QCP_SV, err)) {
                    return FALSE;
                }
                break;
                
            default:
                return qfYamlError(err, &parser, filename,
                                   "internal error: invalid parser state");
        }
        // clean up
        yaml_event_delete(&event);
    }
    
    return TRUE;
}
