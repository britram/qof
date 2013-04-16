#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include <libtrace.h>

void err(const char *fmt, ...)
{
    va_list         ap;
        
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
        
    exit(1);
}

void usage(const char *progname) {
    fprintf(stderr, "usage: %s [-s n] [-p n] [-z n] uri_in uri_out [bpf]\n", progname);
    fprintf(stderr, "-s n : snaplen [0-65535], truncate packets to n bytes.\n");
    fprintf(stderr, "-p n : packet-count, capture n packets then exit.\n");
    fprintf(stderr, "-z n : compression-level [0-9], 0 = no compression.\n");
    exit(2);
}

static char bpfbuf[65536];

void parse_args(int argc,
                char * const argv[],
                const char **uri_in,
                const char **uri_out,
                const char **bpfexpr,
                unsigned int *snaplen,
                unsigned int *tracepkt,
                unsigned int *zlevel)
{
    char *bpfcp = bpfbuf;
    
    /* defaults */
    *snaplen = 65536;
    *tracepkt = 0;
    *zlevel = 0;
    
    char c;
    
    while ((c = getopt(argc, argv, "p:s:z:")) != -1) {
        switch (c) {
            case 'p':
                if (sscanf(optarg, "%u", tracepkt) < 1) {
                    err("bad -p %s", optarg);
                }
                break;
            case 's':
                if (sscanf(optarg, "%u", snaplen) < 1) {
                    err("bad -s %s", optarg);
                }
                if (*zlevel > 65535) {
                    err("bad -s %u", *snaplen);
                }
                break;
            case 'z':
                if (sscanf(optarg, "%u", zlevel) < 1) {
                    err("bad -z %s", optarg);
                }
                if (*zlevel > 9) {
                    err("bad -z %u", *zlevel);
                }
                break;
            case '?':
            default:
                usage(argv[0]);
        }
    }
    argc -= optind;
    argv += optind;
    
    if (argc < 2) usage(argv[0]);
    
    *uri_in = argv[0]; argc--; argv++;
    *uri_out = argv[0]; argc--; argv++;
    
    for(; argc > 0; argc--, argv++) {
        bpfcp = stpncpy(bpfcp, argv[0], 65535 - (bpfcp - bpfbuf));
        if (argc > 1) {
            bpfcp = stpncpy(bpfcp, " ", 65535 - (bpfcp - bpfbuf));
        }
    }
    
    if (bpfcp > bpfbuf) {
        *bpfexpr = bpfbuf;
    } else {
        *bpfexpr = NULL;
    }
}

static unsigned int did_quit = 0;

static void do_quit() {
    did_quit++;
}

static void setup_quit() {
    struct sigaction sa, osa;
    
    /* install quit flag handlers */
    sa.sa_handler = do_quit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT,&sa,&osa)) {
        err("sigaction(SIGINT) failed: %s", strerror(errno));
    }
    
    sa.sa_handler = do_quit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGTERM,&sa,&osa)) {
        err("sigaction(SIGTERM) failed: %s", strerror(errno));
    }
}

int main (int argc, char * const argv[])
{
    const char          *uri_in;
    const char          *uri_out;

    const char          *bpfexpr;
    unsigned int        snaplen;
    unsigned int        tracepkt;
    unsigned int        zlevel;

    unsigned int        pktct = 0;

    libtrace_t          *trace_in;
    libtrace_filter_t   *filter_in;
    libtrace_err_t      terr;

    libtrace_out_t   *trace_out;

    libtrace_packet_t   *packet;

    trace_option_compresstype_t ctype = TRACE_OPTION_COMPRESSTYPE_ZLIB;

    parse_args(argc, argv, &uri_in, &uri_out,
               &bpfexpr, &snaplen, &tracepkt, &zlevel);

    if (!(packet = trace_create_packet())) {
        err("Could not initialize libtrace packet");
    }

    /* set up input */
    trace_in = trace_create(uri_in);
    if (trace_is_err(trace_in)) {
        terr = trace_get_err(trace_in);
        err("Could not open input URI %s: %s", uri_in, terr.problem);
    }
    
    if (trace_config(trace_in, TRACE_OPTION_SNAPLEN, &snaplen) == -1) {
        terr = trace_get_err(trace_in);
        err("Could not set snaplen on input URI %s: %s",
                    uri_in, terr.problem);
    }

    if (bpfexpr) {
        if (!(filter_in = trace_create_filter(bpfexpr))) {
            err("Could not compile filter expression %s", bpfexpr);
        }
    
        if (trace_config(trace_in, TRACE_OPTION_FILTER, filter_in) == -1) {
            terr = trace_get_err(trace_in);
            err("Could not set filter: %s", terr.problem);
        }
    }

    /* set up output */
    trace_out = trace_create_output(uri_out);
    if (trace_is_err_output(trace_out)) {
        terr = trace_get_err_output(trace_out);
        err("Could not open output URI %s: %s", uri_out, terr.problem);
    }

    if (zlevel) {
        if (trace_config_output(trace_out,
                                TRACE_OPTION_OUTPUT_COMPRESSTYPE, &ctype) == -1)
        {
            terr = trace_get_err_output(trace_out);
            err("Could not set compression: %s", terr.problem);
        }
        if (trace_config_output(trace_out,
                                TRACE_OPTION_OUTPUT_COMPRESS, &zlevel) == -1)
        {
            terr = trace_get_err_output(trace_out);
            err("Could not set compression level: %s", terr.problem);
        }
    }

    /* set up quit flag */
    setup_quit();

    /* start output */
    if (trace_start_output(trace_out) == -1) {
        terr = trace_get_err_output(trace_out);
        err("Could not start output: %s", terr.problem);
    }

    /* start input */
    if (trace_start(trace_in) == -1) {
        terr = trace_get_err(trace_in);
        err("Could not start input: %s", terr.problem);
    }
    
    /* print greeting */
    fprintf(stderr, "tracepull starting:\n");
    if (tracepkt) {
        fprintf(stderr, " reading %10u packets from %s", tracepkt, uri_in);
    } else {
        fprintf(stderr, " reading forever from %s", uri_in);
    }
    fprintf(stderr, " snaplen %5u, %s\n",
            snaplen, bpfexpr ? bpfexpr : "unfiltered");
    fprintf(stderr, "  writing %s to %s\n",
            zlevel ? "compressed" : "uncompressed", uri_out);

    /* copy packets from input to output */
    while (!did_quit && trace_read_packet(trace_in, packet) > 0) {
        if (trace_write_packet(trace_out, packet) <= 0) {
            terr = trace_get_err_output(trace_out);
            err("Could not write packet to %s: %s", uri_out, terr.problem);
        }

        /* stop reading packets after nth */
        if (tracepkt && (++pktct >= tracepkt)) break;
    }
            
    /* check for input error */
    if (trace_is_err(trace_in)) {
        terr = trace_get_err(trace_in);
        err("Could not read packet from %s: %s", uri_in, terr.problem);
    }

    /* dump statistics */
    fprintf(stderr, "read       %10llu packets from %s\n", trace_get_accepted_packets(trace_in));
    fprintf(stderr, "  filtered %10llu\n", trace_get_filtered_packets(trace_in));
    fprintf(stderr, "  dropped  %10llu\n", trace_get_dropped_packets(trace_in));
    fprintf(stderr, "wrote      %10u packets to   %s\n", pktct, uri_out);

    /* close up shop */
    trace_destroy(trace_in);
    trace_destroy_output(trace_out);
    trace_destroy_packet(packet);
    if (filter_in) trace_destroy_filter(filter_in);

    return 0;
}
