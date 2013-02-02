/**
 ** qofctx.c
 ** QoF configuration
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2013      Brian Trammell. All Rights Reserved
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell <brian@trammell.ch>
 ** ------------------------------------------------------------------------
 **/

#define _YAF_SOURCE_
#include <yaf/autoinc.h>
#include <yaf/qofifmap.h>

#include <arpa/inet.h>

#include <yaml.h>

#include "qofctx.h"

// FIXME replace this with GError
static void qfYamlError(const yaml_parser_t     *parser,
                        const char              *filename,
                        const char              *errmsg)
{    
    if (!filename) {
        filename = "<no file given>";
    }
    
    if (errmsg) {
        g_warning("QoF config error: %s at line %zd, column %zd in file %s\n",
                errmsg, parser->mark.line, parser->mark.column, filename);
    } else {
        switch (parser->error) {
            case YAML_MEMORY_ERROR:
                g_warning("YAML parser out of memory in file %s.\n", filename);
                break;
            case YAML_READER_ERROR:
                if (parser->problem_value != -1) {
                    g_warning("YAML reader error: %s: #%X at %zd in file %s\n",
                            parser->problem, parser->problem_value,
                            parser->problem_offset, filename);
                } else {
                    g_warning("YAML reader error: %s at %zd in file %s\n",
                            parser->problem, parser->problem_offset, filename);
                }
                break;
            case YAML_SCANNER_ERROR:
            case YAML_PARSER_ERROR:
                if (parser->context) {
                    g_warning("YAML parser error: %s at line %ld, column %ld\n"
                            "%s at line %ld, column %ld in file %s\n",
                            parser->context, parser->context_mark.line+1,
                            parser->context_mark.column+1,
                            parser->problem, parser->problem_mark.line+1,
                            parser->problem_mark.column+1,
                            filename);
                } else {
                    g_warning("YAML parser error: %s at line %ld, column %ld in file %s\n",
                            parser->problem, parser->problem_mark.line+1,
                            parser->problem_mark.column+1,
                            filename);
                }
                break;
            default:
                fprintf(stderr, "YAML parser internal error in file %s.\n", filename);
        }
    }
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
} qcp_state_t;

gboolean qfParseYamlConfig(yfContext_t           *ctx,
                           const char            *filename)
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
        g_warning("cannot open YAML configuration file %s: %s", filename, strerror(errno));
        return FALSE;
    }
    
    memset(&parser, 0, sizeof(parser));
    if (!yaml_parser_initialize(&parser)) {
        qfYamlError(&parser, NULL, NULL);
        return FALSE;
    }
    yaml_parser_set_input_file(&parser, cfgfile);

    while (1) {
        // get next event
        if (!yaml_parser_parse(&parser, &event)) {
            qfYamlError(&parser, filename, NULL);
            return FALSE;
        }
        
        // switch based on state
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        } else if (event.type != YAML_NO_EVENT) switch (qcpstate) {
                
            case QCP_INIT:               // initial state
                if (event.type != YAML_STREAM_START_EVENT) {
                    qfYamlError(&parser, filename,
                                       "internal error: missing stream start");
                    return FALSE;
                }
                qcpstate = QCP_IN_STREAM;
                break;
                
            case QCP_IN_STREAM:          // in stream
                if (event.type == YAML_DOCUMENT_START_EVENT) {
                    qcpstate = QCP_IN_DOC;
                } else if (event.type == YAML_STREAM_END_EVENT) {
                    qcpstate = QCP_INIT;
                } else {
                    qfYamlError(&parser, filename,
                                       "internal error: missing doc start");
                    return FALSE;
                }
                break;
                
            case QCP_IN_DOC:             // in document
                if (event.type == YAML_MAPPING_START_EVENT) {
                    qcpstate = QCP_IN_DOC_MAP;
                } else if (event.type == YAML_DOCUMENT_END_EVENT) {
                    qcpstate = QCP_IN_STREAM;
                } else {
                    qfYamlError(&parser, filename,
                                       "QoF config must consist of a single map");
                    return FALSE;
                }
                break;
                
            case QCP_IN_DOC_MAP:         // in mapping in document
                if (event.type == YAML_MAPPING_END_EVENT) {
                    qcpstate = QCP_IN_DOC;
                } else if (event.type != YAML_SCALAR_EVENT) {
                    qfYamlError(&parser, filename,
                                "internal error: missing key");
                    return FALSE;
                } else if (strcmp((const char*)event.data.scalar.value,
                                  "interface-map") == 0) {
                    qcpstate = QCP_IN_IFMAP;
                } else if (strcmp((const char*)event.data.scalar.value,
                                  "anon-export") == 0) {
                    qcpstate = QCP_IN_ANONMODE;
                } else {
                    qfYamlError(&parser, filename, "unknown configuration key");
                    return FALSE;
                }
                break;
                
            case QCP_IN_IFMAP:           // expecting value (sequence) for interface-map
                if (event.type == YAML_SEQUENCE_START_EVENT) {
                    qcpstate = QCP_IN_IFMAP_SEQ;
                } else {
                    qfYamlError(&parser, filename,
                                "missing sequence value for interface-map");
                    return FALSE;
                }
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
                } else if (event.type == YAML_SEQUENCE_END_EVENT) {
                    qcpstate = QCP_IN_DOC_MAP;
                } else {
                    qfYamlError(&parser, filename,
                                "missing mapping in interface-map");
                    return FALSE;
                }
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
                } else if (event.type != YAML_SCALAR_EVENT) {
                    qfYamlError(&parser, filename,
                                       "internal error: missing key");
                    return FALSE;
                } else if (strcmp((const char*)event.data.scalar.value,
                                  "ip4-net") == 0)
                {
                    qcpstate = QCP_IN_IFMAP_V4NET;
                } else if (strcmp((const char*)event.data.scalar.value,
                                  "ip6-net") == 0)
                {
                    qcpstate = QCP_IN_IFMAP_V6NET;
                } else if (strcmp((const char*)event.data.scalar.value,
                                  "ingress") == 0)
                {
                    qcpstate = QCP_IN_IFMAP_INGRESS;
                } else if (strcmp((const char*)event.data.scalar.value,
                                  "egress") == 0)
                {
                    qcpstate = QCP_IN_IFMAP_EGRESS;
                } else {
                    qfYamlError(&parser, filename,
                                       "unknown interface-map key");
                    return FALSE;
                }
                break;
                
            case QCP_IN_IFMAP_V4NET:    // expecting v4 address value for ifmap
                if (event.type == YAML_SCALAR_EVENT) {
                    
                    rv = sscanf((const char*)event.data.scalar.value,
                                "%15[0-9.]/%u", addr4buf, &pfx4);
                    if (rv == 1) {
                        pfx4 = 32; // implicit single host
                    } else if (rv != 2) {
                        qfYamlError(&parser, filename,
                                           "invalid IPv4 network");
                        return FALSE;
                    }
                    
                    if (pfx4 > 32) {
                        qfYamlError(&parser, filename,
                                           "invalid IPv4 prefix length");
                        return FALSE;
                    }
                    
                    fprintf(stderr, "ip4-net: %s/%u\n", addr4buf, pfx4);
                    
                    rv = inet_pton(AF_INET, addr4buf, &addr4);
                    if (rv == 0) {
                        qfYamlError(&parser, filename,
                                           "invalid IPv4 address");
                        return FALSE;
                    } else if (rv == -1) {
                        qfYamlError(&parser, filename,
                                           strerror(errno));
                        return FALSE;
                    }
                    
                    addr4 = ntohl(addr4);
                    
                    addr4 = addr4;
                    
                } else {
                    qfYamlError(&parser, filename,
                                       "missing value for ip4-net");
                    return FALSE;
                }
                qcpstate = QCP_IN_IFMAP_KEY;
                break;
                
            case QCP_IN_IFMAP_V6NET:    // expecting v6 address value for ifmap
                if (event.type == YAML_SCALAR_EVENT) {
                    rv = sscanf((const char*)event.data.scalar.value,
                                "%39[0-9a-fA-F:.]/%u", addr6buf, &pfx6);
                    if (rv == 1) {
                        pfx6 = 128; // implicit single host
                    } else if (rv != 2) {
                        qfYamlError(&parser, filename,
                                           "invalid IPv6 network");
                        return FALSE;
                    }
                    
                    if (pfx6 > 128) {
                        qfYamlError(&parser, filename,
                                           "invalid IPv6 prefix length");
                        return FALSE;
                    }
                    
                    fprintf(stderr, "ip6-net: %s/%u\n", addr6buf, pfx6);
                    
                    rv = inet_pton(AF_INET6, addr6buf, &addr6);
                    if (rv == 0) {
                        qfYamlError(&parser, filename,
                                           "invalid IPv6 address");
                        return FALSE;
                    } else if (rv == -1) {
                        qfYamlError(&parser, filename,
                                           strerror(errno));
                        return FALSE;
                    }
                    
                } else {
                    qfYamlError(&parser, filename,
                                       "missing value for ip6-net");
                    return FALSE;
                }
                qcpstate = QCP_IN_IFMAP_KEY;
                break;
                
            case QCP_IN_IFMAP_INGRESS:   // expecting ingress interface number for ifmap
                if (event.type == YAML_SCALAR_EVENT) {
                    rv = sscanf((const char *)event.data.scalar.value,
                                "%u", &ingress);
                    if (rv != 1 || ingress > 255) {
                        qfYamlError(&parser, filename,
                                           "invalid ingress interface number");
                        return FALSE;
                    }
                } else {
                    qfYamlError(&parser, filename,
                                       "missing value for ingress-if");
                    return FALSE;
                    
                }
                qcpstate = QCP_IN_IFMAP_KEY;
                break;
                
            case QCP_IN_IFMAP_EGRESS:    // expecting egress interface number for ifmap
                if (event.type == YAML_SCALAR_EVENT) {
                    rv = sscanf((const char *)event.data.scalar.value,
                                "%u", &egress);
                    if (rv != 1 || egress > 255) {
                        qfYamlError(&parser, filename,
                                           "invalid egress interface number");
                        return FALSE;
                    }
                } else {
                    qfYamlError(&parser, filename,
                                       "missing value for egress-if");
                    return FALSE;
                }
                qcpstate = QCP_IN_IFMAP_KEY;
                break;
            
            case QCP_IN_ANONMODE:
                if (event.type == YAML_SCALAR_EVENT) {
                    if (event.data.scalar.value[0] != '0') {
                        yfWriterExportAnon(TRUE);
                    }
                } else {
                    qfYamlError(&parser, filename,
                                "missing value for anon-mode");
                    return FALSE;
                }
                break;
                
            default:
                qfYamlError(&parser, filename,
                                   "internal error: invalid parser state");
                return FALSE;
        }
        // clean up
        yaml_event_delete(&event);
    }
    
    return TRUE;
}
