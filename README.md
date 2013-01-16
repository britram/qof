QoF
===

QoF (Quality of Flow) is a YAF-derived flowmeter, designed for passive
measurement of per-flow performance characteristics. It forked as of YAF
version 2.3.2. It removes all payload inspection and packet handling features 
from YAF, and adds performance measurement and flow pseudonymization features.

QoF is primarily intended to support research into efficient
performance metrics for TCP flows; however, it can also be used for general
flow measurement, especially in environments where the deployment of
technologies which inspect packet payload is restricted. If what you're looking
for is a full-featured enhanced flow meter for security applications, stop
reading now and go download the latest version of YAF, instead.

The present (github master) revision of QoF has not been significantly changed
from YAF aside from the removal of payload inspection and the addition of
initial performance measurement and flow pseudonymization. In particular, the
documentation is still that shipped with YAF 2.3.2; until the initial alpha
release of QoF you'll have to read the code to see what's new.

We intend to track bugfixes in YAF releases subsequent to 2.3.2, but not new
features, especially as new YAF features will likely involve DPI features
removed from QoF. QoF is still mostly command-line compatible with YAF, but
this will probably change in the future, as well.

QoF is licensed under the GNU General Public License, Version 2.
