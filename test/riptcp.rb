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

    # iterate over records
    c.each do |h, m|
        
        # skip if no rtt information
        if !defined? h[:meanTcpRttMilliseconds]
            return
        end
        
        # get address
        if defined?(h[:sourceIPv4Address])
            sip = h[:sourceIPv4Address]
            dip = h[:destinationIPv4Address]
        elsif defined?(h[:sourceIPv6Address])
            sip = h[:sourceIPv6Address]
            dip = h[:destinationIPv6Address]
        else
            STDERR.puts "ignoring record without an address"
            return
        end
        
        puts [sip, dip,
            h[:octetDeltaCount],
            h[:initiatorOctets],
            h[:tcpSequenceCount],
            h[:meanTcpRttMilliseconds],
            h[:maxTcpRttMilliseconds],
            h[:maxTcpFlightSize],
            h[:reverseOctetDeltaCount],
            h[:responderOctets],
            h[:reverseTcpSequenceCount],
            h[:reverseFlowDeltaMilliseconds],
            h[:reverseMeanTcpRttMilliseconds],
            h[:reverseMaxTcpRttMilliseconds],
            h[:reverseMaxTcpFlightSize]].join(", ")
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