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
    uint32_t    sequence;
    uint32_t    export_time;
    uint32_t    delay;
} ipfix_header_t;
    
static size_t read_message(FILE *fp, uint8_t *buf, size_t bufmax) {
    int rv;
    size_t restlen;
    
    if (bufmax < sizeof(ipfix_header_t)) {
        fprintf(stderr, "internal error: message buffer way too small\n");
        exit(1);
    }
    
    if ((rv = fread(buf, sizeof(ipfix_header_t), 1, fp)) != 1) {
        if (rv) {
            fprintf(stderr, "error reading ipfix message header: %s\n",
                    strerror(errno));
            exit(1);            
        } else {
            return 0;
        }
    }
    
    restlen = ntohs(((ipfix_header_t *)buf)->length) - sizeof(ipfix_header_t);
    if ((rv = fread(buf+sizeof(ipfix_header_t), restlen, 1, fp)) != 1) {
        if (rv) {
            fprintf(stderr, "error reading ipfix message body: %s\n",
                    strerror(errno));
            exit(1);            
        }
    }
    
    return ntohs(((ipfix_header_t *)buf)->length);
}

static int get_message_hour(uint8_t *buf, int period) {
    return ntohl(((ipfix_header_t *)buf)->export_time) / period;
}

static FILE *output_for_hour(char *prefix, int hournum, int period) {
    static char fnbuf[4096];
    FILE *outfp = NULL;
    time_t stamp = hournum * period;
    
    strncpy(fnbuf, prefix, sizeof(fnbuf));
    strftime(fnbuf + strlen(prefix), sizeof(fnbuf) - strlen(prefix) - 1,
             "%Y-%m-%d-%h-%m.ipfix", gmtime(&stamp));
    
    if (!(outfp = fopen(fnbuf, "rb"))) {
        fprintf(stderr, "could not open output file %s: %s",
                        fnbuf, strerror(errno));
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
    uint8_t msgbuf[65536];
    size_t msglen;
    FILE *outfp = NULL;
    

    if (argc != 3) {
        fprintf(stderr, "usage: %s period_sec out_prefix < in.ipfix\n",
                        argv[0]);
        exit(1);
    }
    
    period = atoi(argv[1]);
    prefix = argv[2];

    start = time(NULL);

    /* read a message */
    while ((msglen = read_message(stdin, msgbuf, sizeof(msgbuf)))) {
    
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