#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>

typedef struct ipfix_header_st {
    uint16_t    version;
    uint16_t    length;
    uint32_t    export_time;
    uint32_t    sequence;
    uint32_t    domain;
} ipfix_header_t;
    
static size_t read_message(FILE *fp, uint8_t *buf, size_t bufmax) {
    size_t rv = 0;
    size_t restlen = 0;
    size_t header_sz = 16;
    ipfix_header_t *hdr = (ipfix_header_t *)buf;
    
    if (bufmax < header_sz) {
        fprintf(stderr, "internal error: message buffer way too small\n");
        exit(1);
    }
    
    rv = fread(buf, header_sz, 1, fp);
    
    if (rv != 1) {
        if (rv) {
            fprintf(stderr, "error reading ipfix message header: %s\n",
                    strerror(errno));
            exit(1);            
        } else {
            return 0;
        }
    }
    
    restlen = ntohs(hdr->length) - header_sz;
    if (restlen > bufmax - header_sz) {
        fprintf(stderr, "internal error: message buffer too small\n");
        exit(1);
    }
    
    rv = fread(buf+header_sz, restlen, 1, fp);
    
    if (rv != 1) {
        if (rv) {
            fprintf(stderr, "error reading ipfix message body: %s\n",
                    strerror(errno));
            exit(1);            
        }
    }
    
    return ntohs(hdr->length);
}

static int get_message_hour(uint8_t *buf, int period) {
    ipfix_header_t *hdr = (ipfix_header_t *)buf;
    
    return ntohl(hdr->export_time) / period;
}

static FILE *output_for_hour(char *prefix, int hournum, int period) {
    static char fnbuf[4096];
    FILE *outfp = NULL;
    time_t stamp = hournum * period;
    
    strncpy(fnbuf, prefix, sizeof(fnbuf));
    strftime(fnbuf + strlen(prefix), sizeof(fnbuf) - strlen(prefix) - 1,
             "%Y-%m-%d-%H-%M.ipfix", gmtime(&stamp));
    
    if (!(outfp = fopen(fnbuf, "wb"))) {
        fprintf(stderr, "could not open output file %s: %s",
                        fnbuf, strerror(errno));
        exit(1);
    }
    
    return outfp;
}

int main (int argc, char *argv[]) {

    int msgcnt = 0;
    int filecnt = 0;
    int msgpn = 0;
    int lastpn = 0;
    time_t start;
    int period = 3600;
    char* prefix;
    char* infn;
    uint8_t msgbuf[65536];
    size_t msglen;
    FILE *outfp = NULL;
    FILE *infp = NULL;
    

    if (argc < 3) {
        fprintf(stderr, "usage: %s period_sec out_prefix [in.ipfix] [< in.ipfix]\n",
                        argv[0]);
        exit(1);
    }
    
    period = atoi(argv[1]);
    prefix = argv[2];
    
    if (argc == 4) {
        infn = argv[3];
        infp = fopen(infn, "rb");
        if (!infp) {
            fprintf(stderr, "could not open input file %s: %s",
                    infn, strerror(errno));
            exit(1);
        }
    } else {
        infn = "-";
        infp = stdin;
    }

    start = time(NULL);

    /* read a message */
    while ((msglen = read_message(infp, msgbuf, sizeof(msgbuf)))) {
        
        /* get message hour number */
        msgpn = get_message_hour(msgbuf, period);

        /* rotate output if necessary */
        if (msgpn > lastpn) {
            if (outfp) {
                fclose(outfp);
            }
        
            outfp = output_for_hour(prefix, msgpn, period);
            filecnt++;
            lastpn = msgpn;
        }
    
        /* write message */
        fwrite(msgbuf, msglen, 1, outfp);
        msgcnt++;
    }


    /* print statistics */
    time_t elapsed = time(NULL) - start;
    fprintf(stderr, "wrote %u messages to %u files in %ld sec (%.2f m/s).\n", 
                    msgcnt, filecnt, elapsed, (double)msgcnt / elapsed);
    return 0;
}