###
# generic IPFIX to pandas dataframe
#

import ipfix.ie
import ipfix.reader
import pandas as pd
import collections
from ipaddress import ip_network

def dataframe_from_ipfix(filename, *ienames):
    """ 
    read an IPFIX file into a dataframe, selecting only records
    containing all the named IEs
     
    """
    ielist = ipfix.ie.spec_list(ienames)
    
    with open(filename, mode="rb") as f:
        r = ipfix.reader.from_stream(f)
        df = pd.DataFrame.from_records(
            [rec for rec in r.records_as_tuple(ielist)],
            columns = [ie.name for ie in ielist])
        return df    

def dataframe_derive_duration(df):
    """
    add a floating point duration in seconds column 
    to a dataframe including the time_ies
    
    """
    try:
        df['durationSeconds'] = (df['flowEndMilliseconds'] - 
                                 df['flowStartMilliseconds']).map(
                                         lambda x: x.item()/1e9)
    except KeyError:
        pass
        
def dataframe_derive_nets(df, v4_prefix=16, v6_prefix=64):
    """
    add columns to the given dataframe for uniform networks given a prefix.
    allows aggregation by network.
    """
    try:
        df["sourceIPv4Network"] = df["sourceIPv4Address"].map(
                lambda x: ip_network(str(x)+"/"+str(v4_prefix), strict=False))
    except KeyError:
        pass
    
    try:
        df["destinationIPv4Network"] = df["destinationIPv4Address"].map(
                lambda x: ip_network(str(x)+"/"+str(v4_prefix), strict=False))
    except KeyError:
        pass

    try:
        df["sourceIPv6Network"] = df["sourceIPv6Address"].map(
                lambda x: ip_network(str(x)+"/"+str(v6_prefix), strict=False))
    except KeyError:
        pass
    
    try:
        df["destinationIPv6Network"] = df["destinationIPv6Address"].map(
                lambda x: ip_network(str(x)+"/"+str(v6_prefix), strict=False))
    except KeyError:
        pass

        
key_ies = ("flowID",
            "sourceIPv4Address",
            "destinationIPv4Address",
            "sourceTransportPort",
            "destinationTransportPort",
            "protocolIdentifier")

time_ies = ("flowStartMilliseconds",
            "flowEndMilliseconds")

address_ies = ("sourceIPv4Address")

count_ies = ("octetDeltaCount", 
             "packetDeltaCount",
             "initiatorOctets", 
             "initiatorPackets",
             "tcpSequenceCount",
             "tcpSequenceLossCount")

rtt_ies = ("meanTcpRttMilliseconds",
           "minTcpRttMilliseconds")

rtx_ies = ("tcpRetransmitCount", 
           "tcpRtxBurstCount")

rev_count_ies = ("reverseOctetDeltaCount", 
                 "reversePacketDeltaCount",
                 "responderOctets",
                 "responderPackets",
                 "reverseTcpSequenceCount",
                 "reverseTcpSequenceLossCount")

rev_rtt_ies = ("reverseMeanTcpRttMilliseconds",
               "reverseMinTcpRttMilliseconds")

rev_rtx_ies = ("reverseTcpRetransmitCount",
               "reverseTcpRtxBurstCount")

    