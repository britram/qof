import argparse
import bz2
from sys import stdin, stdout, stderr

import ipfix
import qof
import pandas as pd

args = None

def characteristic_present(df, flag):
    return ((df["qofTcpCharacteristics"] & flag) == flag) | \
           ((df["reverseQofTcpCharacteristics"] & flag) == flag)

def ecn_negotiated(df):
    ecn_syn = qof.TCP_SYN | qof.TCP_ECE | qof.TCP_CWR
    ecn_ack = qof.TCP_SYN | qof.TCP_ACK | qof.TCP_ECE
    ecn_mask = ecn_syn | ecn_ack
    return ((df["initialTCPFlags"] & ecn_mask) == ecn_syn) & \
           ((df["reverseInitialTCPFlags"] & ecn_ack) == ecn_ack)
    
def print_proportion(feature, feature_s):
    try:
        print ("%-10s observed on %8u flows (%8.5f%%)" % 
               (feature,
                feature_s.value_counts()[True],
                feature_s.value_counts()[True] * 100 / len(feature_s)))
        return feature_s.value_counts()[True]
    except:
        print ("%-10s not observed" % feature)

def ip4_sources(df):
    return pd.concat((df['sourceIPv4Address'], 
                      df['destinationIPv4Address']), ignore_index=1).unique()

def ip4_sources_given(df, fcond_s, rcond_s):
    csrc_fwd = df[fcond_s]["sourceIPv4Address"]
    csrc_rev = df[rcond_s]["destinationIPv4Address"]
    return pd.concat((csrc_fwd, csrc_rev), ignore_index=1).unique()

def ip4_sources_characteristic(df, flag):
    return ip4_sources_given(df, (df["qofTcpCharacteristics"] & flag) == flag, 
                                 (df["reverseQofTcpCharacteristics"] & flag) == flag)

def parse_args():
    global args
    parser = argparse.ArgumentParser(description="Report on options usage in a QoF-produced IPFIX file")
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
    df = qof.dataframe_from_ipfix_stream(ipfix_stream, 
            ("initialTCPFlags","reverseInitialTCPFlags",
             "unionTCPFlags","reverseUnionTCPFlags",
             "qofTcpCharacteristics", "reverseQofTcpCharacteristics",
             "tcpSequenceLossCount", "reverseTcpSequenceLossCount",
             "packetDeltaCount", "reversePacketDeltaCount",
             "sourceIPv4Address", "destinationIPv4Address"))

    print ("Total flows:         "+str(len(df)))
    df = qof.drop_lossy(df)
    print ("  of which lossless: "+str(len(df)))
    df = qof.drop_incomplete(df)
    print ("  of which complete: "+str(len(df)))
    
    ecn_nego_s = ecn_negotiated(df)    
    ecn_ect0_s = characteristic_present(df, qof.QOF_ECT0)
    ecn_ect1_s = characteristic_present(df, qof.QOF_ECT1)
    ecn_ce_s = characteristic_present(df, qof.QOF_CE)

    print_proportion("ECN nego", ecn_nego_s)
    print_proportion("ECT0", ecn_ect0_s)
    print_proportion("ECT1", ecn_ect1_s)
    print_proportion("nego->ECT0", ecn_nego_s & ecn_ect0_s)
    print_proportion("nego->ECT1", ecn_nego_s & ecn_ect1_s)
    print_proportion("CE", ecn_ce_s)
    print()
    print_proportion("ECT0+ECT1", ecn_ect0_s & ecn_ect1_s)
    print_proportion("ECT0+CE", ecn_ce_s & ecn_ect0_s)
    print_proportion("ECT1+CE", ecn_ce_s & ecn_ect1_s)
    print_proportion("any ECx", ecn_ce_s | ecn_ect0_s | ecn_ect1_s)
    print_proportion("all ECx", ecn_ce_s & ecn_ect0_s & ecn_ect1_s)
    print()
        
    tcp_ws_s = characteristic_present(df, qof.QOF_WS)
    tcp_ts_s = characteristic_present(df, qof.QOF_TS)
    tcp_sack_s = characteristic_present(df, qof.QOF_SACK)

    print_proportion("WS", tcp_ws_s)
    print_proportion("TS", tcp_ts_s)
    print_proportion("SACK", tcp_sack_s)
    print()
    
    all_sources = ip4_sources(df)
    ws_sources = ip4_sources_characteristic(df, qof.QOF_WS)
    print("WS   observed from %8u sources (%8.5f%%)" % 
          (len(ws_sources), len(ws_sources) * 100 / len(all_sources)))
    ts_sources = ip4_sources_characteristic(df, qof.QOF_TS)
    print("TS   observed from %8u sources (%8.5f%%)" % 
          (len(ts_sources), len(ts_sources) * 100 / len(all_sources)))
    sack_sources = ip4_sources_characteristic(df, qof.QOF_SACK)
    print("SACK observed from %8u sources (%8.5f%%)" % 
          (len(sack_sources), len(sack_sources) * 100 / len(all_sources)))

    nego_sources = ip4_sources_given(df, ecn_nego_s, ecn_nego_s)
    print("ECN nego involved %8u sources (%8.5f%%)" % 
          (len(nego_sources), len(nego_sources) * 100 / len(all_sources)))

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