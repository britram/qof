import ipfix.ie
import ipfix.reader
import ipfix.message

import numpy as np 
import pandas as pd
import matplotlib.pyplot as plt

import itertools
import socketserver
import argparse
import csv
import bz2
from sys import stdin, stdout, stderr
from datetime import datetime

args = None

def parse_args():
    global args
    parser = argparse.ArgumentParser(description="Convert an IPFIX file or stream to CSV")
    parser.add_argument('--spec', '-s', metavar="specfile", action="append",
                        help="file to load additional IESpecs from")
    parser.add_argument('--file', '-f', metavar="file", nargs="?",
                        help="IPFIX file to read (default stdin)")
    parser.add_argument('--bzip2', '-j', action="store_const", const=True,
                        help="Decompress bz2-compressed IPFIX file")
    parser.add_argument('--collect', '-c', metavar="transport",
                        help="run CP on specified transport")
    parser.add_argument('--bind', '-b', metavar="bind",
                        default="", help="address to bind to as CP (default all)")
    parser.add_argument('--port', '-P', metavar="port", type=int,
                        default=4739, help="port to bind to as CP (default 4739)")
    parser.add_argument('--rotate-rec', metavar="records", type=int,
                        default=100000, help="number of records per output file")
    parser.add_argument('--out', metavar="dir",
                        default=".", help="write PNG files to directory (default .)")
    args = parser.parse_args()

def init_ipfix(specfiles = None):
    ipfix.types.use_integer_ipv4()
    ipfix.ie.use_iana_default()
    ipfix.ie.use_5103_default()
    
    if specfiles:
        for sf in specfiles:
            ipfix.ie.use_specfile(sf)

def iter_group(iterable, n, fillvalue=None):
    zipargs = [iter(iterable)] * n
    return itertools.zip_longest(*zipargs, fillvalue=fillvalue)

def _dataframe_iterator(tuple_iterator, columns, chunksize=100000):
    for group in iter_group(tuple_iterator, chunksize):
        yield pd.DataFrame.from_records([rec for rec in 
                  filter(lambda a: a is not None, group)], columns=columns)

def plot_rtt_spectrum(df, filename):
    plt.figure(figsize=(8,8))
    df["minTcpRttMilliseconds"].hist(bins=125,
                                     range=(1,501),
                                     weights=df["transportPacketDeltaCount"]+
                                             df["reverseTransportPacketDeltaCount"])
    plt.savefig(filename)

def stream_to_scinet(instream):
    global args
    # select IEs to import
    ienames = ["sourceIPv4Address", "destinationIPv4Address", 
               "minTcpRttMilliseconds", "tcpRttSampleCount",
               # "tcpLossEventCount", 
               "transportPacketDeltaCount", "reverseTransportPacketDeltaCount",
               # "tcpSequenceCount", "reverseTcpSequenceCount",
               # "transportOctetDeltaCount", "reverseTransportOctetDeltaCount"]
               ]
    ielist = ipfix.ie.spec_list(ienames)
    r = ipfix.reader.from_stream(instream)
    i = r.tuple_iterator(ielist)

    # iterate over dataframes
    for df in _dataframe_iterator(i, ienames, args.rotate_rec):
        plot_rtt_spectrum(df, args.out + 
                  datetime.today().strftime("/rtt_%d%H%M%S.png"))

class TcpScinetHandler(socketserver.StreamRequestHandler):
    def handle(self):
        stderr.write("connection from "+str(self.client_address)+"\n")
        stream_to_scinet(self.rfile)

if __name__ == "__main__":
    # set "args" global
    parse_args()

    init_ipfix(args.spec)

    if args.collect == 'tcp':
        stderr.write("starting TCP CP on "+args.bind+":"+
                         str(args.port)+"; Ctrl-C to stop\n")
        stderr.flush()
        ss = socketserver.TCPServer((args.bind, args.port), TcpScinetHandler)
        ss.serve_forever()
    elif args.collect:
        raise ValueError("Unsupported transport "+args.collect+"; must be 'tcp'")
    elif args.file:
        if args.bzip2:
            with bz2.open (args.file, mode="rb") as f:
                stream_to_scinet(f)
        else:
            with open (args.file, mode="rb") as f:
                stream_to_scinet(f)
    else:
        stdin = stdin.detach()
        stream_to_scinet(stdin)
