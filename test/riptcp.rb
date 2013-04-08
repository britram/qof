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
    
    puts ["synrtt","l3pkt","l7pkt",
          "l3oct","l4oct","l7oct",
          "rtx","maxooo","maxif",
          "minrtt","rtt","mss",
          "minttl","maxttl"].map { |s| "%7s"%(s) }.join(", ")

    # iterate over records
    c.each do |h, m|
        # skip if no rtt information
        unless h[:lastTcpRttMilliseconds]
          next
        end
        
        if h[:octetDeltaCount] > 0
          puts [
            h[:reverseFlowDeltaMilliseconds],
            h[:packetDeltaCount],
            h[:initiatorPackets],
            h[:octetDeltaCount],
            h[:initiatorOctets],
            h[:tcpSequenceCount],
            h[:tcpRetransmitCount],
            h[:maxTcpReorderSize],
            h[:maxTcpInflightSize],
            h[:minTcpRttMilliseconds],
            h[:lastTcpRttMilliseconds],
            h[:observedTcpMss],
            h[:minimumTTL],
            h[:maximumTTL]].map { |s| "%7u"%(s) }.join(", ")
        end
        
        if h[:reverseOctetDeltaCount] > 0
          puts [ 
            0,
            h[:reversePacketDeltaCount],
            h[:responderPackets],
            h[:reverseOctetDeltaCount],
            h[:responderOctets],
            h[:reverseTcpSequenceCount],
            h[:reverseTcpRetransmitCount],
            h[:reverseMaxTcpReorderSize],
            h[:reverseMaxTcpInflightSize],
            h[:reverseMinTcpRttMilliseconds],
            h[:reverseLastTcpRttMilliseconds],
            h[:reverseObservedTcpMss],
            h[:reverseMinimumTTL],
            h[:reverseMaximumTTL]].map { |s| "%7u"%(s) }.join(", ")
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