#!/usr/bin/env ruby1.9
#
#--
# ripfix (IPFIX for Ruby) (c) 2010 Brian Trammell and Hitachi Europe SAS
# Special thanks to the PRISM Consortium (fp7-prism.eu) for its support.
# Distributed under the terms of the GNU Lesser General Public License v3.
#++
# Dumps an IPFIX file on standard input for debugging purposes. Takes one
# optional argument on the command line, the name of an information model file
# to extend the default model
#

require 'ipfix/collector'
require 'ipfix/iana'

include IPFIX

def fixread(c)
    
    c.session.on_missing_template do |message, set|
        STDERR.puts "***** missing template for #{set.set_id} in domain #{message.domain} *****"
    end
    
    c.session.on_bad_sequence do |message, expected|
        STDERR.puts " **** bad sequence for domain #{message.domain}: got #{message.sequence}, expected #{expected} ****"
    end
    
    puts ["fid","l3pkt","l7pkt",
          "l3oct","l4oct","l7oct",
          "rtx","ooo","maxooo","maxflight",
          "minrtt","rtt","mss"].map { |s| "%7s"%(s) }.join(", ")

    # iterate over records
    c.each do |h, m|
        # skip if no rtt information
        unless (h[:meanTcpRttMilliseconds] || h[:reverseMeanTcpRttMilliseconds])
          next
        end

        # skip lossy flows
        if (h[:tcpSequenceLossCount] + h[:reverseTcpSequenceLossCount]) > 0
            next
        end

        # skip non-mss flows
        if h[:observedTcpMss] < 1400 && h[:reverseObservedTcpMss] < 1400
            next
        end
        
        # skip small flows
        if (h[:initiatorPackets] + h[:responderPackets]) < 66
          next
        end
        
        if h[:octetDeltaCount] > 0
          puts [
            h[:flowId],
            h[:packetDeltaCount],
            h[:initiatorPackets],
            h[:octetDeltaCount],
            h[:initiatorOctets],
            h[:tcpSequenceCount],
            h[:tcpRetransmitCount],
            h[:tcpOutOfOrderCount],
            h[:maxTcpReorderSize],
            h[:maxTcpFlightSize],
            h[:minTcpRttMilliseconds],
            h[:meanTcpRttMilliseconds],
            h[:observedTcpMss]].map { |s| s ? "%7u"%(s) : "     na" }.join(", ")
        end
        
        if h[:reverseOctetDeltaCount] > 0
          puts [ 
            h[:flowId],
            h[:reversePacketDeltaCount],
            h[:responderPackets],
            h[:reverseOctetDeltaCount],
            h[:responderOctets],
            h[:reverseTcpSequenceCount],
            h[:reverseTcpRetransmitCount],
            h[:reverseTcpOutOfOrderCount],
            h[:reverseMaxTcpReorderSize],
            h[:reverseMaxTcpFlightSize],
            h[:reverseMinTcpRttMilliseconds],
            h[:reverseMeanTcpRttMilliseconds],
            h[:reverseObservedTcpMss]].map { |s| s ? "%7u"%(s) : "     na" }.join(", ")
        end
    end
end

# arg globals
modelfiles = Array.new
fixfiles = Array.new

# parse args
while arg = ARGV.shift
if (argm = /^model:(\S+)/.match(arg))
        modelfiles.push(argm[1])
    else
        fixfiles.push(arg)
    end
end

# load model
model = InfoModel.new.load_default.load_reverse
modelfiles.each do |modelfile|
    STDERR.puts("loading information model file #{modelfile}")
    model.load(modelfile)
end

# process files
fixfiles.each do |f|
    fixread(FileReader.new(f, model))
end