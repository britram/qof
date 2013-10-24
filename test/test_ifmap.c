//
//  test_ifmap.c
//  qof
//
//  Created by Brian Trammell on 1/18/13.
//  Copyright (c) 2013 Brian Trammell. All rights reserved.
//

#define _YAF_SOURCE_
autoinc.h>
qofifmap.h>

#include <arpa/inet.h>

#include <yaml.h>

void print_parser_error(FILE* out, const yaml_parser_t *parser, const char *filename, const char *errmsg) {

    if (!filename) {
        filename = "<no file given>";
    }
    
    if (errmsg) {
        fprintf(out, "QoF config error: %s at line %zd, column %zd in file %s\n",
                errmsg, parser->mark.line, parser->mark.column, filename);
    } else {
        switch (parser->error) {
        case YAML_MEMORY_ERROR:
            fprintf(out, "YAML parser out of memory in file %s.\n", filename);
            break;
        case YAML_READER_ERROR:
            if (parser->problem_value != -1) {
                fprintf(out, "YAML reader error: %s: #%X at %zd in file %s\n",
                        parser->problem, parser->problem_value,
                        parser->problem_offset, filename);
            } else {
                fprintf(out, "YAML reader error: %s at %zd in file %s\n",
                        parser->problem, parser->problem_offset, filename);
            }
            break;
        case YAML_SCANNER_ERROR:
        case YAML_PARSER_ERROR:
            if (parser->context) {
                fprintf(out, "YAML parser error: %s at line %ld, column %ld\n"
                        "%s at line %ld, column %ld in file %s\n",
                        parser->context, parser->context_mark.line+1,
                        parser->context_mark.column+1,
                        parser->problem, parser->problem_mark.line+1,
                        parser->problem_mark.column+1,
                        filename);
            } else {
                fprintf(out, "YAML parser error: %s at line %ld, column %ld in file %s\n",
                        parser->problem, parser->problem_mark.line+1,
                        parser->problem_mark.column+1,
                        filename);
            }
        break;
        default:
            fprintf(out, "YAML parser internal error in file %s.\n", filename);
        }
    }
}

typedef enum {
    QCP_INIT,               // initial state
    QCP_IN_STREAM,          // in stream
    QCP_IN_DOC,             // in document
    QCP_IN_DOC_MAP,         // in mapping in document
    QCP_IN_IFMAP,           // expecting value (sequence) for interface-map
    QCP_IN_IFMAP_SEQ,       // in value (sequence) for interface-map
    QCP_IN_IFMAP_KEY,       // expecting key for interface-map
    QCP_IN_IFMAP_V4NET,    // expecting v4 address value for ifmap
    QCP_IN_IFMAP_V6NET,    // expecting v6 address value for ifmap
    QCP_IN_IFMAP_INGRESS,   // expecting ingress interface number for ifmap
    QCP_IN_IFMAP_EGRESS,    // expecting egress interface number for ifmap
    QCP_IN_DOC_SKIP         // skipping a non-interface 
} qcp_state_t;

int parse_ifmap(qfIfMap_t *map, yaml_parser_t *parser, const char *filename) {
    
    qcp_state_t     qcpstate = QCP_INIT;
    yaml_event_t    event;

    char            addr4buf[16];
    uint32_t        addr4;
    unsigned int    pfx4;

    char            addr6buf[40];
    uint8_t         addr6[16];
    unsigned int    pfx6;
    
    unsigned int    ingress;
    unsigned int    egress;
    
    int             rv;

    while (1) {
        // get next event
        if (!yaml_parser_parse(parser, &event)) {
            print_parser_error(stderr, parser, filename, NULL);
            return 1;
        }
        
        // switch based on state
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        } else if (event.type != YAML_NO_EVENT) switch (qcpstate) {

            case QCP_INIT:               // initial state
                if (event.type != YAML_STREAM_START_EVENT) {
                    print_parser_error(stderr, parser, filename,
                                       "internal error: missing stream start");
                    return 1;
                }
                qcpstate = QCP_IN_STREAM;
                break;

            case QCP_IN_STREAM:          // in stream
                if (event.type == YAML_DOCUMENT_START_EVENT) {
                    qcpstate = QCP_IN_DOC;
                } else if (event.type == YAML_STREAM_END_EVENT) {
                    qcpstate = QCP_INIT;
                } else {
                    print_parser_error(stderr, parser, filename,
                                       "internal error: missing doc start");
                    return 1;
                }
                break;

            case QCP_IN_DOC:             // in document
                if (event.type == YAML_MAPPING_START_EVENT) {
                    qcpstate = QCP_IN_DOC_MAP;
                } else if (event.type == YAML_DOCUMENT_END_EVENT) {
                    qcpstate = QCP_IN_STREAM;
                } else {
                    print_parser_error(stderr, parser, filename,
                                       "QoF config must consist of a single map:"
                                       " sequence not allowed");
                    return 1;
                }
                break;
                
            case QCP_IN_DOC_MAP:         // in mapping in document
                if (event.type == YAML_SCALAR_EVENT) {
                    if (strcmp((const char*)event.data.scalar.value, "interface-map") == 0) {
                        qcpstate = QCP_IN_IFMAP;
                    } else {
                        print_parser_error(stderr, parser, filename,
                                           "unknown configuration key");
                        return 1;
                    }
                } else if (event.type == YAML_MAPPING_END_EVENT) {
                    qcpstate = QCP_IN_DOC;
                } else {
                    print_parser_error(stderr, parser, filename,
                                       "internal error: missing key");
                    return 1;
                }
                break;
                
            case QCP_IN_IFMAP:           // expecting value (sequence) for interface-map
                if (event.type == YAML_SEQUENCE_START_EVENT) {
                    qcpstate = QCP_IN_IFMAP_SEQ;
                } else {
                    print_parser_error(stderr, parser, filename,
                                       "missing sequence value for interface-map");
                    return 1;
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
                    print_parser_error(stderr, parser, filename,
                                       "missing mapping in interface-map");
                    return 1;
                }
                break;
                    
            case QCP_IN_IFMAP_KEY:       // expecting key for interface-map
                if (event.type == YAML_MAPPING_END_EVENT) {
                    // end mapping, add buffers to map
                    if (addr4buf[0]) {
                        qfIfMapAddIPv4Mapping(map, addr4, pfx4, ingress, egress);
                    }
                    if (addr6buf[0]) {
                        qfIfMapAddIPv6Mapping(map, addr6, pfx6, ingress, egress);
                    }
                    qcpstate = QCP_IN_IFMAP_SEQ;
                } else if (event.type != YAML_SCALAR_EVENT) {
                    print_parser_error(stderr, parser, filename,
                                       "internal error: missing key");
                    return 1;
                } else if (strcmp((const char*)event.data.scalar.value, "ip4-net") == 0) {
                    qcpstate = QCP_IN_IFMAP_V4NET;
                } else if (strcmp((const char*)event.data.scalar.value, "ip6-net") == 0){
                    qcpstate = QCP_IN_IFMAP_V6NET;
                } else if (strcmp((const char*)event.data.scalar.value, "ingress") == 0) {
                    qcpstate = QCP_IN_IFMAP_INGRESS;
                } else if (strcmp((const char*)event.data.scalar.value, "egress") == 0) {
                    qcpstate = QCP_IN_IFMAP_EGRESS;
                } else {
                    print_parser_error(stderr, parser, filename,
                                       "unknown interface-map key");
                    return 1;
                }
                break;
                
            case QCP_IN_IFMAP_V4NET:    // expecting v4 address value for ifmap
                if (event.type == YAML_SCALAR_EVENT) {
                    
                    rv = sscanf((const char*)event.data.scalar.value, "%15[0-9.]/%u", addr4buf, &pfx4);
                    if (rv == 1) {
                        pfx4 = 32; // implicit single host
                    } else if (rv != 2) {
                        print_parser_error(stderr, parser, filename,
                                           "invalid IPv4 network");
                        return 1;
                    }
                    
                    if (pfx4 > 32) {
                        print_parser_error(stderr, parser, filename,
                                           "invalid IPv4 prefix length");
                        return 1;
                    }

                    fprintf(stderr, "ip4-net: %s/%u\n", addr4buf, pfx4);
                    
                    rv = inet_pton(AF_INET, addr4buf, &addr4);
                    if (rv == 0) {
                        print_parser_error(stderr, parser, filename,
                                           "invalid IPv4 address");
                        return 1;
                    } else if (rv == -1) {
                        print_parser_error(stderr, parser, filename,
                                           strerror(errno));
                        return 1;
                    }
                    
                    addr4 = ntohl(addr4);
                    
                    addr4 = addr4;
                    
                } else {
                    print_parser_error(stderr, parser, filename,
                                       "missing value for ip4-net");
                    return 1;
                }
                qcpstate = QCP_IN_IFMAP_KEY;
                break;
                    
            case QCP_IN_IFMAP_V6NET:    // expecting v6 address value for ifmap
                if (event.type == YAML_SCALAR_EVENT) {
                    rv = sscanf((const char*)event.data.scalar.value, "%39[0-9a-fA-F:.]/%u", addr6buf, &pfx6);
                    if (rv == 1) {
                        pfx6 = 128; // implicit single host
                    } else if (rv != 2) {
                        print_parser_error(stderr, parser, filename,
                                           "invalid IPv6 network");
                        return 1;
                    }
                    
                    if (pfx6 > 128) {
                        print_parser_error(stderr, parser, filename,
                                           "invalid IPv6 prefix length");
                        return 1;
                    }
                    
                    fprintf(stderr, "ip6-net: %s/%u\n", addr6buf, pfx6);
                    
                    rv = inet_pton(AF_INET6, addr6buf, &addr6);
                    if (rv == 0) {
                        print_parser_error(stderr, parser, filename,
                                           "invalid IPv6 address");
                        return 1;
                    } else if (rv == -1) {
                        print_parser_error(stderr, parser, filename,
                                           strerror(errno));
                        return 1;
                    }

                } else {
                    print_parser_error(stderr, parser, filename,
                                       "missing value for ip6-net");
                    return 1;
                }
                qcpstate = QCP_IN_IFMAP_KEY;
                break;
                    
            case QCP_IN_IFMAP_INGRESS:   // expecting ingress interface number for ifmap
                if (event.type == YAML_SCALAR_EVENT) {
                    rv = sscanf((const char *)event.data.scalar.value, "%u", &ingress);
                    if (rv != 1 || ingress > 255) {
                        print_parser_error(stderr, parser, filename,
                                           "invalid ingress interface number");
                        return 1;
                    }
                } else {
                    print_parser_error(stderr, parser, filename,
                                       "missing value for ingress-if");
                    return 1;
                    
                }
                qcpstate = QCP_IN_IFMAP_KEY;
                break;

            case QCP_IN_IFMAP_EGRESS:    // expecting egress interface number for ifmap
                if (event.type == YAML_SCALAR_EVENT) {
                    rv = sscanf((const char *)event.data.scalar.value, "%u", &egress);
                    if (rv != 1 || egress > 255) {
                        print_parser_error(stderr, parser, filename,
                                           "invalid egress interface number");
                        return 1;
                    }
                } else {
                    print_parser_error(stderr, parser, filename,
                                       "missing value for egress-if");
                    return 1;
                    
                }
                qcpstate = QCP_IN_IFMAP_KEY;
                break;
                
            case QCP_IN_DOC_SKIP:        // skipping a non-ifmap key (TODO)
            default:
                print_parser_error(stderr, parser, filename,
                                   "internal error: invalid parser state");
                return 1;
        }
        // clean up
        yaml_event_delete(&event);
    }
    
    return 0;
}

int main (int argc, char* argv[])
{
    qfIfMap_t       map;
    yaml_parser_t   parser;
    
    FILE            *mapfile, *testfile;
    
    static char     testbuf[1024];
    static char     addrbuf[40];
    
    yfFlowKey_t     testkey;
    
    uint32_t        addr4;
    uint8_t         ingress, egress;
    
    int rv;
    
    // check args
    if (argc != 3) {
        fprintf(stdout, "Usage: test_ifmap mapfile testfile\n");
        return 1;
    }
    
    // initialize map
    qfIfMapInit(&map);

    // initilize parser
    memset(&parser, 0, sizeof(parser));
    if (!yaml_parser_initialize(&parser)) {
        print_parser_error(stderr, &parser, NULL, NULL);
        return 1;
    }
    
    // open and bind mapfile
    if (!(mapfile = fopen(argv[1], "r"))) {
        fprintf(stderr, "error opening mapfile: %s\n", strerror(errno));
        return 1;
    }
    yaml_parser_set_input_file(&parser, mapfile);

    // parse the mapfile
    fprintf(stdout, "Parsing mapfile %s...\n", argv[1]); 
    if ((rv = parse_ifmap(&map, &parser, argv[1])) != 0) {
        return rv;
    }
    
    fprintf(stdout, "Mapfile parse successful:\n");
    
    qfIfMapDump(stdout, &map);
    
    // open and parse the testfile
    if (!(testfile = fopen(argv[2], "r"))) {
        fprintf(stderr, "error opening testfile: %s\n", strerror(errno));
        return 1;
    }
    
    while (fgets(testbuf, sizeof(testbuf), testfile)) {

        // initialize the testkey
        memset(&testkey, 0, sizeof(testkey));

        if (strchr(testbuf, ':')) {
            if ((sscanf(testbuf, " %39[0-9a-fA-F:.]", addrbuf) == 1) &&
                (inet_pton(AF_INET6, addrbuf, testkey.addr.v6.sip) == 1))
            {
                testkey.version = 6;
                memcpy(testkey.addr.v6.dip, testkey.addr.v6.sip, sizeof(testkey.addr.v6.dip));
                inet_ntop(AF_INET6, testkey.addr.v6.sip, addrbuf, sizeof(addrbuf));
            } else {
                fprintf(stdout, "can't parse IPv6 address %40s\n", testbuf);
                continue;
            }
        } else {
            if ((sscanf(testbuf, " %15[0-9.]", addrbuf) == 1) &&
                (inet_pton(AF_INET, addrbuf, &addr4) == 1))
            {
                testkey.version = 4;
                testkey.addr.v4.sip = ntohl(addr4);
                testkey.addr.v4.dip = ntohl(addr4);
                inet_ntop(AF_INET, &addr4, addrbuf, sizeof(addrbuf));
            } else {
                fprintf(stdout, "can't parse IPv4 address %40s\n", testbuf);
                continue;
            }
        }
        
        qfIfMapAddresses(&map, &testkey, &ingress, &egress);
        fprintf(stdout, "%40s in %3u out %3u\n", addrbuf, ingress, egress);
    }
}
