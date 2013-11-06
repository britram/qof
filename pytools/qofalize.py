import argparse
import itertools
import bz2
from sys import stdin, stdout, stderr

import ipfix
import qof
import pandas as pd
import numpy as np

args = None

def parse_args():
    global args
    parser = argparse.ArgumentParser(description="Generate time series aggregates of a QoF-produced IPFIX file")
    parser.add_argument('--spec', '-s', metavar="specfile", action="append",
                        help="file to load additional IESpecs from")
    parser.add_argument('--file', '-f', metavar="file",
                        help="IPFIX file to read (default stdin)")
    parser.add_argument('--bzip2', '-j', action="store_const", const=True,
                        help="Decompress bz2-compressed IPFIX file")
    parser.add_argument('--bin', '-b', metavar="sec", type=int, default=300,
                        help="Bin to use for time series resampling [300 = 5min]")
    parser.add_argument('--uniflow', action="store_const", const=True,
                       help="Assume uniflow input")
    parser.add_argument('--sequence', action="store_const", const=True,
                        help="Measure sequence numbers and TCP loss")
    parser.add_argument('--obsloss', action="store_const", const=True,
                        help="Measure observation loss")
    args = parser.parse_args()
 
def init_ipfix(specfiles = None):
    ipfix.types.use_integer_ipv4()
    ipfix.ie.use_iana_default() 
    ipfix.ie.use_5103_default()

    if specfiles:
        for sf in specfiles:
            ipfix.ie.use_specfile(sf)
            
def select_ies(args):
    # select base information elements
    ienames = ["flowStartMilliseconds", "flowEndMilliseconds",
               "packetDeltaCount", "transportPacketDeltaCount", 
               "octetDeltaCount", "transportOctetDeltaCount"]

    # select reverse information elements
    if not args.uniflow:
        ienames.extend(["reversePacketDeltaCount",
                        "reverseTransportPacketDeltaCount",
                        "reverseOctetDeltaCount",
                        "reverseTransportOctetDeltaCount"])

    # select sequence count information elements
    if args.sequence:
        ienames.append("tcpSequenceCount")
        if not args.uniflow:
            ienames.append("reverseTcpSequenceCount")

    # select obsloss information elements
    if args.obsloss:
        ienames.append("tcpSequenceLossCount")
        if not args.uniflow:
            ienames.append("reverseTcpSequenceLossCount")

    return ienames
            
def qofalize(ipfix_stream, ienames, binsize=300):
    df = qof.dataframe_from_ipfix_stream(ipfix_stream, ienames)
    
    # basic data rate
    bps = df["octetDeltaCount"]
    if "reverseOctetDeltaCount" in ienames:
        bps += df["reverseOctetDeltaCount"]
    bps *= 8 / binsize
    bps.index = df["flowEndMilliseconds"]
    dfo = pd.DataFrame({'bps': bps})
    
    # packet rate
    pps = df["packetDeltaCount"]
    if "reversePacketDeltaCount" in ienames:
        pps += df["reversePacketDeltaCount"]
    pps /= binsize
    pps.index = df["flowEndMilliseconds"]
    dfo['pps'] = pps
    
    # resample
    dfo = dfo.resample(str(binsize)+"S", how=sum)
    
    # and print    
    dfo.to_csv(stdout)
        
## begin main ##

parse_args()
init_ipfix(args.spec)

if args.file:
    if args.bzip2:
        with bz2.open (args.file, mode="rb") as f:
            qofalize(f, select_ies(args), args.bin)
    else:
        with open (args.file, mode="rb") as f:
            qofalize(f, select_ies(args), args.bin)
else:
    stdin = stdin.detach()
    qofalize(stdin, select_ies(args), args.bin)
