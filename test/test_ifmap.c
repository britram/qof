//
//  test_ifmap.c
//  qof
//
//  Created by Brian Trammell on 1/18/13.
//  Copyright (c) 2013 Brian Trammell. All rights reserved.
//

#define _YAF_SOURCE_
#include <yaf/autoinc.h>
#include <yaf/qofifmap.h>

#include <arpa/inet.h>

#include <yaml.h>

void print_parser_error(FILE* out, const yaml_parser_t *parser, const char *filename, const char *errmsg) {

    if (!filename) {
        filename = "<no file given>";
    }
    
    if (errmsg) {
        fprintf(out, "QoF config error: %s at at line %d, column %d in file %s\n",
                errmsg, parser->mark.line, parser->mark.column, filename);
    } else {
        switch (parser->error) {
        case YAML_MEMORY_ERROR:
            fprintf(out, "YAML parser out of memory in file %s.\n", filename);
            break;
        case YAML_READER_ERROR:
            if (parser->problem_value != -1) {
                fprintf(out, "YAML reader error: %s: #%X at %d in file %s\n",
                        parser->problem, parser->problem_value, parser->problem_offset, filename);
            } else {
                fprintf(out, "YAML reader error: %s at %d in file %s\n",
                        parser->problem, parser->problem_offset, filename);
            }
            break;
        case YAML_SCANNER_ERROR:
        case YAML_PARSER_ERROR:
            if (parser->context) {
                fprintf(out, "YAML parser error: %s at line %d, column %d\n"
                        "%s at line %d, column %d in file %s\n",
                        parser->context, parser->context_mark.line+1, parser->context_mark.column+1,
                        parser->problem, parser->problem_mark.line+1, parser->problem_mark.column+1,
                        filename);
            } else {
                fprintf(out, "YAML parser error: %s at line %d, column %d in file %s\n",
                        parser->problem, parser->problem_mark.line+1, parser->problem_mark.column+1,
                        filename);
            }
        break;
        default:
            fprintf(out, "YAML parser internal error in file %s.\n", filename);
        }
    }
}

typedef enum {
    QCP_INIT,
    QCP_IN_STREAM,
    QCP_IN_DOC,
    QCP_IN_DOC_MAP,
    QCP_IN_IFMAP,
    QCP_IN_IFMAP_SEQ,
    QCP_IN_IFMAP_MAP,
    QCP_IN_IFMAP_V4ADDR,
    QCP_IN_IFMAP_V6ADDR,
    QCP_IN_IFMAP_PREFIX,
    QCP_IN_IFMAP_INGRESS,
    QCP_IN_IFMAP_EGRESS,
} qcp_state_t;

int main (int argc, char* argv[])
{
    qfIfMap_t       map;
    yaml_parser_t   parser;
    yaml_event_t    event;
    qcp_state_t     qcpstate = QCP_INIT;
    
    uint8_t addr6[16];
    
    // check args
    if (argc != 3) {
        fprintf(stdout, "Usage: test_ifmap mapfile testfile\n");
        return 1;
    }

    // initilize parser
    memset(&parser, 0, sizeof(parser));
    if (!yaml_parser_initialize(&parser)) {
        print_parser_error(stderr, &parser, NULL, NULL);
        return 1;
    }
    
    if (!(mapfile = fopen(argv[1], "r"))) {
        fprintf(stderr, "error opening mapfile: %s\n", strerror(errno));
        return 1;
    }
    
    yaml_parser_set_input_file(&parser, mapfile);

    while (!mapfile_done) {
        if (!yaml_parser_parse(&parser, &event)) {
            print_parser_error(stderr, &parser, argv[1], NULL);
            return 1;
        }
        
        if (event.type == YAML_STREAM_END_EVENT) {
            break;
        }
        
        switch(event.type) {
                
        // Stream bookkeeping events
        case YAML_STREAM_START_EVENT:
            if (qcpstate == QCP_INIT) {
                qcpstate = QCP_IN_STREAM;
            } else {
                print_parser_error(stderr, &parser, argv[1],
                                   "internal error: unexpected stream start");
                return 1;
            }
            break;
        case YAML_STREAM_END_EVENT:
            if (qcpstate == QCP_IN_STREAM) {
                qcpstate = QCP_INIT;
            } else {
                print_parser_error(stderr, &parser, argv[1],
                                   "unexpected stream end");
                return 1;
            }
            break;
        case YAML_DOCUMENT_START_EVENT:
            if (qcpstate == QCP_IN_STREAM) {
                qcpstate = QCP_IN_DOC;
            } else {
                print_parser_error(stderr, &parser, argv[1],
                                   "internal error: unexpected document start");
                return 1;
            }
            break;
        case YAML_DOCUMENT_END_EVENT:
            if (qcpstate == QCP_IN_DOC) {
                qcpstate = QCP_IN_STREAM;
            } else {
                print_parser_error(stderr, &parser, argv[1],
                                   "unexpected document end");
                return 1;
            }
            break;
        case YAML_MAPPING_START_EVENT:
            if (qcpstate == QCP_IN_DOC) {
                qcpstate = QCP_IN_DOC_MAP;
            } elsif (qcpstate == QCP_IN_IFMAP_SEQ) {
                qcpstate = QCP_IN_IFMAP_MAP;
            } else {
                print_parser_error(stderr, &parser, argv[1],
                                   "unexpected map start");
                return 1;
            }
            break;
        case YAML_MAPPING_END_EVENT:
            if (qcpstate == QCP_IN_DOC_MAP) {
                qcpstate = QCP_IN_DOC;
            } elsif (qcpstate == QCP_IN_IFMAP_MAP) {
                qcpstate = QCP_IN_IFMAP_SEQ;
            } else {
                print_parser_error(stderr, &parser, argv[1],
                                   "unexpected map end");
                return 1;
            }
            break;
                
        // FIXME WORK POINTER HERE
            
            case YAML_NO_EVENT: puts("No event!"); break;
                /* Stream start/end */
            case YAML_SEQUENCE_START_EVENT: puts("<b>Start Sequence</b>"); break;
            case YAML_SEQUENCE_END_EVENT:   puts("<b>End Sequence</b>");   break;
            case YAML_MAPPING_START_EVENT:  puts("<b>Start Mapping</b>");  break;
            case YAML_MAPPING_END_EVENT:    puts("<b>End Mapping</b>");    break;
                /* Data */
            case YAML_ALIAS_EVENT:  printf("Got alias (anchor %s)\n", event.data.alias.anchor); break;
            case YAML_SCALAR_EVENT: printf("Got scalar (value %s)\n", event.data.scalar.value); break;
        }

    }
    
    
    // initialize map
    qfIfMapInit(&map);
    

    
    
    
    qfIfMapAddIPv4Mapping(&map,0x0A000100,24,1,11);
    qfIfMapAddIPv4Mapping(&map,0x0A000200,24,2,12);
    qfIfMapAddIPv4Mapping(&map,0x0A000300,24,3,13);
    qfIfMapAddIPv4Mapping(&map,0x0A000400,24,4,14);
    qfIfMapAddIPv4Mapping(&map,0x7f000000,8,9,19);
    
    inet_pton(AF_INET6, "2003:db8:1:2::0", addr6);
    qfIfMapAddIPv6Mapping(&map, addr6, 64, 2, 12);
    
    // FIXME run a few tests
}