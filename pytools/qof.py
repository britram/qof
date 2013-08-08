###
# generic IPFIX to pandas dataframe
# and some QoF-specific tools

import ipfix.ie
import ipfix.reader
import pandas as pd
import collections
from ipaddress import ip_network
from datetime import datetime, timedelta

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

def drop_lossy(df):
    """
    Filter out any rows for which observation loss was detected.
    
    Returns a copy of the dataframe without lossy flows

    """
    try:
        lossy = df['tcpSequenceLossCount'] + df['reverseTcpSequenceLossCount'] > 0
        return df[lossy == False]
    except KeyError:
        return df
    
    
def derive_duration(df):
    """
    add a floating point duration column
    to a dataframe including flowStartMilliseconds and flowEndMilliseconds
    
    modifies the dataframe in place and returns it.
    """
    try:
        df['durationSeconds'] = (df['flowEndMilliseconds'] - 
                                 df['flowStartMilliseconds']).map(
                                         lambda x: x.item()/1e9)
    except KeyError:
        pass
    
    return df
        
def derive_nets(df, v4_prefix=16, v6_prefix=64):
    """
    add columns to the given dataframe for uniform networks given a prefix.
    allows aggregation by network.
    
    modifies the dataframe in place and returns it.
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
    
    return df

               
def index_by_key_timeout(df, timeout=timedelta(seconds=15), 
                         keycol="sourceIPv4Address", 
                         timecol="flowStartMilliseconds"):
    """
    Implement vector aggregation of flows into groups given a key column,
    a time column, and a timeout. Sorts by the key and time columns, then 
    
    Returns a sorted, reindexed copy of the dataframe.
    
    """
    # Created a sorted dataframe indexed in sort order.
    sdf = df.sort((keycol, timecol))
    sdf.index = pd.Index(range(len(sdf)))
    
    # Add temporary columns for key and time; 
    # shift these up by one via reindexing
    prevk = sdf[keycol]
    prevk.index = pd.Index(range(1,len(prevk)+1))
    sdf['prevk'] = prevk
    
    prevt = sdf[timecol]
    prevt.index = pd.Index(range(1,len(prevt)+1))
    sdf['prevt'] = prevt
    
    # Derive a "regroup signal": start a new group any time the key changes
    # or the time column has a difference greater than the timeout
    regroup = (sdf[keycol] != sdf['prevk']) | \
               (sdf[timecol] - sdf['prevt'] > timeout)
    
    # Now build two new index columns based on this regroup signal
    gids = []
    next_gid = 0
    next_gidx = 0

    for z in regroup.values:
        if z:
            next_gid += 1
            next_gidx = 0
        gids.append((next_gid, next_gidx))
        next_gidx += 1
        
    # And make them the new index
    sdf.index = pd.MultiIndex.from_tuples(gids, names=["gid", "gidx"])

    return sdf
    
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
             "transportOctetDeltaCount", 
             "transportPacketDeltaCount",
             "tcpSequenceCount",
             "tcpSequenceLossCount")

rtt_ies = ("meanTcpRttMilliseconds",
           "minTcpRttMilliseconds")

rtx_ies = ("tcpRetransmitCount", 
           "tcpRtxBurstCount")

rev_count_ies = ("reverseOctetDeltaCount", 
                 "reversePacketDeltaCount",
                 "reverseTransportOctetDeltaCount",
                 "reverseTransportPacketDeltaCount",
                 "reverseTcpSequenceCount",
                 "reverseTcpSequenceLossCount")

rev_rtt_ies = ("reverseMeanTcpRttMilliseconds",
               "reverseMinTcpRttMilliseconds")

rev_rtx_ies = ("reverseTcpRetransmitCount",
               "reverseTcpRtxBurstCount")

    