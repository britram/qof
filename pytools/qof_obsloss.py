import argparse
import bz2
from sys import stdin, stdout, stderr

import ipfix
import qof
import pandas as pd
import numpy as np

args = None

def parse_args():
    global args
    parser = argparse.ArgumentParser(description="Report on observation loss in a QoF-produced IPFIX file")
    parser.add_argument('--spec', '-s', metavar="specfile", action="append",
                        help="file to load additional IESpecs from")
    parser.add_argument('--file', '-f', metavar="file",
                        help="IPFIX file to read (default stdin)")
    parser.add_argument('--bin', '-b', metavar="sec", type=int, default=None,
                        help="Output binned flow and octet loss rates")
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
      
def obsloss_report_stream_biflow(ipfix_stream, timeslice = 0):
    df = qof.dataframe_from_ipfix_stream(ipfix_stream, (
	    "flowStartMilliseconds", "flowEndMilliseconds",
        "packetDeltaCount", "reversePacketDeltaCount", 
        "transportPacketDeltaCount", "reverseTransportPacketDeltaCount",
        "octetDeltaCount", "reverseOctetDeltaCount",
        "transportOctetDeltaCount", "reverseTransportOctetDeltaCount",
        "tcpSequenceCount", "reverseTcpSequenceCount",
        "tcpSequenceLossCount", "reverseTcpSequenceLossCount"))
    
    allcount = len(df)
    df["lossy"] = (df["tcpSequenceLossCount"] > 0) | (df["reverseTcpSequenceLossCount"] > 0)
    lossycount = len(df[df["lossy"]])

    print ("Total flows:      %u " % (allcount))
    print ("  of which lossy: %u (%.2f%%)" % (lossycount, lossycount * 100 / allcount))

    if timeslice:
        lossy_flows = np.where(df["lossy"], 1, 0)
        lossy_flows.index = df["flowEndMilliseconds"]
        total_flows = pd.Series(1, index=lossy_flows.index)
        lossy_flows = lossy_flows.resample(str(timeslice)+"S", how='sum')
        total_flows = total_flows.resample(str(timeslice)+"S", how='sum')
        lossy_flow_rate = lossy_flows / total_flows;
        dfout = pd.DataFrame({'total': total_flows, 
                              'lossy': lossy_flows, 
                              'rate': lossy_flow_rate})
        dfout.to_csv(stdout)
        
## begin main ##

parse_args()
init_ipfix(args.spec)

if args.file:
    if args.bzip2:
        with bz2.open (args.file, mode="rb") as f:
            obsloss_report_stream_biflow(f, args.bin)
    else:
        with open (args.file, mode="rb") as f:
            obsloss_report_stream_biflow(f, args.bin)
else:
    stdin = stdin.detach()
    obsloss_report_stream_biflow(stdin, args.bin)
