QoF
===

QoF (Quality of Flow) is an IPFIX Metering and Exporting process derived from
the YAF flowmeter, designed for passive measurement of per-flow
performance characteristics.

QoF is primarily intended to support research into passive measurement of
performance metrics for TCP flows; however, it can also be used for general
flow measurement, especially in environments where the deployment of
technologies which inspect packet payload is restricted. If what you're looking
for is a full-featured flow meter for security applications, stop reading now
and go download the latest version of YAF from http://tools.netsa.cert.org/yaf
instead.

Changes from YAF
================

QoF is a fork of YAF version 2.3.2, with the following major differences 
from the YAF codebase:

- Removal of all payload inspection code.

- Replacement of packet acquisition layer with WAND's libtrace.

- Replacement of most command line flags with a YAML-based configuration file,
  which allows implicit feature selection through direct specification of the
  information elements to appear in QoF's export templates.
  
- Support for new information elements focused on passive TCP performance
  measurement.

QoF is licensed under the GNU General Public License, Version 2.

Usage
=====

Run without arguments, QoF will read packets from standard input and produce
IPFIX on standard output using a minimal biflow template. For more detailed
usage instructions, see the man page installed with QoF, or
http://britram.github.io/qof/qof.pdf.
