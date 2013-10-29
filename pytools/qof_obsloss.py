import argparse
import bz2
from sys import stdin, stdout, stderr

import ipfix
import qof
import pandas as pd

args = None

def parse_args():
    global args
    parser = argparse.ArgumentParser(description="Report on observation loss in a QoF-produced IPFIX file")
    parser.add_argument('--spec', '-s', metavar="specfile", action="append",
                        help="file to load additional IESpecs from")
    parser.add_argument('--file', '-f', metavar="file",
                        help="IPFIX file to read (default stdin)")
    parser.add_argument('--bzip2', '-j', action="store_const", const=True,
                        help="Decompress bz2-compressed IPFIX file")
    args = parser.parse_args()
 
def init_ipfix(specfiles = None):
    ipfix.types.use_integer_ipv4()
    ipfix.ie.use_iana_default() 
    ipfix.ie.use_5103_default()

    if specfiles:
        for sf in specfiles:
            ipfix.ie.use_specfile(sf)
      
def opts_report_stream(ipfix_stream):
    df = qof.dataframe_from_ipfix_stream(ipfix_stream, (
	    "flowStartMilliseconds", "flowEndMilliseconds",
        "packetDeltaCount", "reversePacketDeltaCount", 
        "transportPacketDeltaCount", "reverseTransportPacketDeltaCount",
        "octetDeltaCount", "reverseOctetDeltaCount",
        "transportOctetDeltaCount", "reverseTransportOctetDeltaCount",
        "tcpSequenceCount", "reverseTcpSequenceCount",
        "tcpSequenceLossCount", "reverseTcpSequenceLossCount"))
    
    flowcount = len(df)
    df["lossy"] = (df["tcpSequenceLossCount"] > 0) | (df["reverseTcpSequenceLossCount"] > 0)
    lossycount = len(df[df["lossy"]])

    print ("Total flows:      %u " % (allcount))
    print ("  of which lossy: %u %.2f" % (lossycount, lossycount * 100 / allcount))

parse_args()
init_ipfix(args.spec)

if args.file:
    if args.bzip2:
        with bz2.open (args.file, mode="rb") as f:
            opts_report_stream(f)
    else:
        with open (args.file, mode="rb") as f:
            opts_report_stream(f)
else:
    stdin = stdin.detach()
    opts_report_stream(stdin)
