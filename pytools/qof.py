###
# generic IPFIX to pandas dataframe
#

import ipfix.ie
import ipfix.reader

import pandas as pd

import collections

def dataframe_from_ipfix(filename, *ienames):
    
    ielist = ipfix.ie.spec_list(ienames)
    
    with open(filename, mode="rb") as f:
        r = ipfix.reader.from_stream(f)
        df = pd.DataFrame.from_records(
            [rec for rec in r.records_as_tuple(ielist)],
            columns = [ie.name for ie in ielist])
        return df

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

rev_rtx_ies ("reverseTcpRetransmitCount",
             "reverseTcpRtxBurstCount")