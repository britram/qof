## ------------------------------------------------------------------------
## Makefile.am (documentation)
## autotools build system for Airframe
## ------------------------------------------------------------------------
## Copyright (C) 2005-2011 Carnegie Mellon University. All Rights Reserved.
## ------------------------------------------------------------------------
## Authors: Brian Trammell 
## ------------------------------------------------------------------------
## GNU Lesser GPL Rights pursuant to Version 2.1, February 1999
## Government Purpose License Rights (GPLR) pursuant to DFARS 252.227-7013
## ------------------------------------------------------------------------

man1_MANS = airdaemon.1 filedaemon.1
PODFILES = airdaemon.pod filedaemon.pod

.pod.1:
	$(POD2MAN) $(AIRFRAME_POD2MAN_ARGS) --section=1 $< $@

MOSTLYCLEANFILES = $(man1_MANS)

EXTRA_DIST =    $(PODFILES)                      \
                $(man1_MANS)                     \
html/airlock_8h-source.html \
html/airlock_8h.html \
html/airopt_8h-source.html \
html/airopt_8h.html \
html/airutil_8h-source.html \
html/airutil_8h.html \
html/annotated.html \
html/daeconfig_8h-source.html \
html/daeconfig_8h.html \
html/doxygen.css \
html/doxygen.png \
html/files.html \
html/functions.html \
html/functions_vars.html \
html/globals.html \
html/globals_eval.html \
html/globals_func.html \
html/globals_type.html \
html/globals_vars.html \
html/index.html \
html/logconfig_8h-source.html \
html/logconfig_8h.html \
html/mio_8h-source.html \
html/mio_8h.html \
html/mio__config_8h-source.html \
html/mio__config_8h.html \
html/mio__sink__file_8h-source.html \
html/mio__sink__file_8h.html \
html/mio__sink__multi_8h-source.html \
html/mio__sink__multi_8h.html \
html/mio__sink__tcp_8h-source.html \
html/mio__sink__tcp_8h.html \
html/mio__sink__udp_8h-source.html \
html/mio__sink__udp_8h.html \
html/mio__source__file_8h-source.html \
html/mio__source__file_8h.html \
html/mio__source__pcap_8h-source.html \
html/mio__source__pcap_8h.html \
html/mio__source__tcp_8h-source.html \
html/mio__source__tcp_8h.html \
html/mio__source__udp_8h-source.html \
html/mio__source__udp_8h.html \
html/mio__stdio_8h-source.html \
html/mio__stdio_8h.html \
html/privconfig_8h-source.html \
html/privconfig_8h.html \
html/struct___air_lock.html \
html/struct___m_i_o_app_driver.html \
html/struct___m_i_o_sink.html \
html/struct___m_i_o_sink_file_config.html \
html/struct___m_i_o_source.html \
html/struct___m_i_o_source_file_config.html \
html/struct___m_i_o_source_p_cap_file_config.html \
html/struct___m_i_o_source_p_cap_live_config.html \
html/struct___m_i_o_source_t_c_p_config.html \
html/tab_b.gif \
html/tab_l.gif \
html/tab_r.gif \
html/tabs.css \
html/tools.css
