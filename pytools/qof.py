import ipfix.ie
import ipfix.reader

import collections

RttPair = collections.namedtuple("RttPair", "sip", "dip", "rtt", "revrtt")

rtt_pair_ies = ipfix.ie.spec_list("sourceIPv4Address",
                                  "destinationIPv4Address",
                                  "meanTcpRttMilliseconds",
                                  "reverseMeanTcpRttMilliseconds")