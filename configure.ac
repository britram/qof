dnl Process this file with autoconf to produce a configure script
dnl ------------------------------------------------------------------------
dnl configure.in
dnl autotools build system for YAF applications
dnl ------------------------------------------------------------------------
dnl Copyright (C) 2012-2013 Brian Trammell.             All Rights Reserved.
dnl Copyright (C) 2005-2012 Carnegie Mellon University. All Rights Reserved.
dnl ------------------------------------------------------------------------
dnl Authors: Brian Trammell
dnl ------------------------------------------------------------------------
dnl GNU General Public License (GPL) Rights pursuant to Version 2, June 1991
dnl Government Purpose License Rights (GPLR) pursuant to DFARS 252.227-7013
dnl ------------------------------------------------------------------------

dnl RCSIDENT("$Id: configure.in 18549 2012-09-14 15:57:18Z ecoff_svn $")

AC_INIT([qof], [0.9.0], [brian@trammell.ch], [qof])
AC_CONFIG_AUX_DIR([autoconf])
AC_COPYRIGHT([Copyright (C) 2012-2013 by Brian Trammell
Copyright (C) 2006-2012 by Carnegie Mellon University
GNU Public License (GPL) Rights pursuant to Version 2, June 1991
Government Purpose License Rights (GPLR) pursuant to DFARS 252.227-7013])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([1.6.3 foreign])
LIBCOMPAT="4:0:0"
AC_REVISION($Revision: 18549 $)
AC_CONFIG_HEADERS(include/yaf/config.h)
AC_CONFIG_HEADERS(airframe/include/airframe/config.h)

RPM_CONFIG_FLAGS=""
OPTION_CONFIG_STRING="build configuration: "

dnl ----------------------------------------------------------------------
dnl Check for (or install) libltdl #AC_LIBTOOL_DLOPEN
dnl ----------------------------------------------------------------------

AC_LIBTOOL_DLOPEN

m4_include([autoconf/ax_check_ltdl.m4])
AC_PATH_LIBLTDL

AC_PROG_LIBTOOL

AC_CONFIG_SUBDIRS(libltdl)

dnl -------------------------------------
dnl set version of plugins
dnl -------------------------------------
version_info="1:1:0"
AC_SUBST([version_info])

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_HEADER_STDC
AC_LANG([C])

dnl ----------------------------------------------------------------------
dnl Set up doxygen
dnl ----------------------------------------------------------------------
m4_include([autoconf/ax_prog_doxygen.m4])

DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN(qof, Doxyfile, doc)

dnl and then there was an ancient version of redhate, AS4
dnl AC_PROG_CC_C99

if test "x$CPPFLAGS" == "x"; then
    CPPFLAGS='-Wall -Wno-comment -Wno-long-long'
fi
CPPFLAGS='-I$(top_builddir)/include -I$(top_srcdir)/include -I$(top_builddir)/airframe/include -I$(top_srcdir)/airframe/include'" $CPPFLAGS"

AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_HEADER_TIME

AC_CHECK_HEADERS([errno.h fcntl.h glob.h signal.h sys/errno.h grp.h malloc.h])
AC_CHECK_HEADERS([netdb.h netinet/in.h pwd.h stdarg.h stddef.h sys/socket.h syslog.h])
AC_CHECK_HEADERS([inttypes.h limits.h])


AC_SEARCH_LIBS([nanosleep], [rt])
AC_SEARCH_LIBS([inet_ntoa], [nsl])
AC_SEARCH_LIBS([socket], [socket])
AC_SEARCH_LIBS([log], [m])

AC_SUBST(YAF_REQ_AIRFRAME_VER, [2.0.0])

dnl ----------------------------------------------------------------------
dnl Set up pod2man
dnl ----------------------------------------------------------------------

AC_CHECK_PROGS(POD2MAN, [pod2man true])

YAF_POD2MAN_ARGS="--center='QoF' --release='"${PACKAGE_VERSION}"' --date='"`date +"%v"`"'"
AIRFRAME_POD2MAN_ARGS="--center='airframe' --release='"${PACKAGE_VERSION}"' --date='"`date +"%v"`"'"

AC_SUBST(YAF_POD2MAN_ARGS)
AC_SUBST(AIRFRAME_POD2MAN_ARGS)

dnl ----------------------------------------------------------------------
dnl Set up pod2html
dnl ----------------------------------------------------------------------
AC_CHECK_PROGS(POD2HTML,pod2html)

dnl ----------------------------------------------------------------------
dnl Check for aligned access required
dnl ----------------------------------------------------------------------
m4_include([autoconf/ax_check_aligned_access_required.m4])

AX_CHECK_ALIGNED_ACCESS_REQUIRED


dnl ----------------------------------------------------------------------
dnl Modify PKG_CONFIG_PATH based on prefix
dnl ----------------------------------------------------------------------

if test "x$PKG_CONFIG_PATH" != "x"; then
    PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"
    export PKG_CONFIG_PATH
else
    PKG_CONFIG_PATH="$prefix/lib/pkgconfig"
    export PKG_CONFIG_PATH
fi

dnl ----------------------------------------------------------------------
dnl Check for glib
dnl ----------------------------------------------------------------------

GLIB_PRESENT=NO
GLIB_LDADD=

AM_PATH_GLIB_2_0([2.4.7],[GLIB_PRESENT=YES])

AC_ARG_WITH(glib-static,
AS_HELP_STRING([--with-glib-static=prefix],[use static glib tree]),[
GLIB_STATIC_LIBGLIB2_A="${withval}/lib/libglib-2.0.a"
GLIB_STATIC_INCLUDE="${withval}/include/glib-2.0"
GLIB_STATIC_LIBINCLUDE="${withval}/lib/glib-2.0/include"
if test -f ${GLIB_STATIC_LIBGLIB2_A}; then
    if test -f "${GLIB_STATIC_INCLUDE}/glib.h"; then
        GLIB_LDADD=${GLIB_STATIC_LIBGLIB2_A}
        GLIB_CFLAGS="-I${GLIB_STATIC_INCLUDE} -I${GLIB_STATIC_LIBINCLUDE}"
        AC_SUBST(GLIB_LIBS)
        AC_SUBST(GLIB_CFLAGS)
        GLIB_PRESENT=YES
        AC_MSG_NOTICE([assuming static glib in ${withval} is good...])
        OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}"glib from ${withval} |"
    fi
fi
])

if test x"$GLIB_PRESENT" != "xYES"; then
  AC_MSG_ERROR([Cannot find a suitable glib2 (>= 2.4.7)])
fi

AC_SUBST(GLIB_LDADD)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)
AC_SUBST(LIBLTDL)
AC_SUBST(LTDLINCL)

use_popt=no

AC_ARG_WITH(popt, AS_HELP_STRING([--with-popt=path],[location of popt]),
[
    use_popt=yes
    if test "x${with_popt}" == "x"; then
         if test -d ${withval}; then
             POPT_CLFAGS="-I${withval}/include"
             POPT_LDFLAGS="-L${withval}/lib -lpopt"
             CFLAGS="$CFLAGS ${POPT_CFLAGS}"
             CPPFLAGS="$CFLAGS ${POPT_CFLAGS}"
             LDFLAGS="$LDFLAGS ${POPT_LDFLAGS}"
         fi
    fi
    RPM_CONFIG_FLAGS="${RPM_CONFIG_FLAGS} --with-popt=${with_popt}"
])

if test "x$use_popt" = "xyes"; then
    AC_CHECK_LIB(popt,poptParseArgvString, [
        AC_MSG_NOTICE([using libpopt found in ${with_popt} for options processing])
        AC_DEFINE([USE_POPT], 1,
          [Define to 1 to use libpopt])
          OPTION_CONFIG_SRING=${OPTION_CONFIG_STRING}"popt from ${with_popt} |"],[
            AC_MSG_ERROR([libpopt was not found or is not usable])
        ]
    )
else
    if $PKG_CONFIG --atleast-version 2.6.4 glib-2.0 >/dev/null 2>&1 ; then
        AC_MSG_NOTICE([using GLib for options processing])
        AC_DEFINE(USE_GOPTION, 1, [Define to 1 to enable use of GOption])
        AC_SUBST(YAF_REQ_GLIB_VER, [2.6.4])
        OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}"glib opt processing|"
    else
        AC_SUBST(YAF_REQ_GLIB_VER, [2.4.7])
        AC_CHECK_LIB(popt,poptParseArgvString, [
            AC_MSG_NOTICE([using libpopt found in ${with_popt} for options processing])
            POPT_LDFLAGS="-lpopt"
            LDFLAGS="$LDFLAGS ${POPT_LDFLAGS}"
            AC_DEFINE([USE_POPT], 1,
              [Define to 1 to use libpopt])
              OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}"popt from ${with_popt}|"],[
                AC_MSG_ERROR([Unable to find a suitable library for options processing])
            ]
        )
    fi
fi

dnl ----------------------------------------------------------------------
dnl figure out the right format string for printing size_t
dnl ----------------------------------------------------------------------
YF_SIZE_T_FORMAT

dnl ----------------------------------------------------------------------
dnl Check for libfixbuf
dnl when changing the version number required, do both the subst, and
dnl package lines!
dnl ----------------------------------------------------------------------
AC_SUBST(YAF_REQ_FIXBUF_VER, [1.0.0])
PKG_CHECK_MODULES(libfixbuf, [libfixbuf >= 1.0.0],
                  [
                    YF_PKGCONFIG_VERSION(libfixbuf)
                    YF_PKGCONFIG_LPATH(libfixbuf)
                    OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}"fixbuf "${yfpkg_ver}" "${yfpkg_lpath}"|"
                  ],
                  [AC_MSG_ERROR([Cannot find a suitable libfixbuf (>= 1.0.0) (Try setting PKG_CONFIG_PATH): $libfixbuf_PKG_ERRORS])
])

dnl ----------------------------------------------------------------------
dnl Check for libyaml
dnl ----------------------------------------------------------------------
AC_ARG_WITH(libyaml,
    AS_HELP_STRING([--with-libyaml=YAML_DIR],[find yaml.h in YAML_DIR/include, libyaml in YAML_DIR/lib]),[
    if test "x$withval" != "xno"; then
        if test -d ${withval}; then
            if test -d ${withval}/lib; then
                CFLAGS="-I${withval}/include $CFLAGS"
                CPPFLAGS="-I${withval}/include $CPPFLAGS"
                LIBS="-L${withval}/lib -lpcap $LIBS"
                if test "x$target_os" = "xlinux" -o \
                        "x$target_os" = "xlinux-gnu"; then
                    LIBS="-Wl,--rpath -Wl,${withval}/lib $LIBS"
                fi
            else
                LIBS="-L${withval} -lpcap $LIBS"
                if test "x$target_os" = "xlinux" -o \
                        "x$target_os" = "xlinux-gnu"; then
                    LIBS="-Wl,--rpath -Wl,${withval} $LIBS"
                fi
            fi
            OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}"libyaml from ${withval}|"
            RPM_CONFIG_FLAGS="${RPM_CONFIG_FLAGS} --with-libyaml=${withval}"
        fi
        AC_CHECK_LIB(yaml, yaml_parser_initialize, ,
            [AC_MSG_ERROR([cannot locate libyaml, try --with-libyaml])])

        AC_CHECK_HEADERS(yaml.h,[],
            [AC_MSG_ERROR([cannot locate yaml.h, try --with-libyaml])])
    fi
    ],[
    
    AC_CHECK_LIB(yaml, yaml_parser_initialize, ,
        [AC_MSG_ERROR([cannot locate libyaml, try --with-libyaml])])

    AC_CHECK_HEADERS(yaml.h,[],
        [AC_MSG_ERROR([cannot locate yaml.h, try --with-libyaml])])
    ]
)

dnl ----------------------------------------------------------------------
dnl Check for libpcap
dnl ----------------------------------------------------------------------
AC_ARG_WITH(libpcap,
    AS_HELP_STRING([--with-libpcap=PCAP_DIR],[find pcap.h in PCAP_DIR/include, libpcap in PCAP_DIR/lib]),[
    if test "x$withval" != "xno"; then
        if test -d ${withval}; then
            if test -d ${withval}/lib; then
                CFLAGS="-I${withval}/include $CFLAGS"
                CPPFLAGS="-I${withval}/include $CPPFLAGS"
                if test "x$target_os" = "xcygwin"; then
                    LIBS="-L${withval}/lib -lwpcap $LIBS"
                else
                    LIBS="-L${withval}/lib -lpcap $LIBS"
                    if test "x$target_os" = "xlinux" -o \
                            "x$target_os" = "xlinux-gnu"; then
                        LIBS="-Wl,--rpath -Wl,${withval}/lib $LIBS"
                    fi
                fi
            else
                if test "x$target_os" = "xcygwin"; then
                    LIBS="-L${withval} -lwpcap $LIBS"  
                else
                    LIBS="-L${withval} -lpcap $LIBS"
                    if test "x$target_os" = "xlinux" -o \
                            "x$target_os" = "xlinux-gnu"; then
                        LIBS="-Wl,--rpath -Wl,${withval} $LIBS"
                    fi
                fi
            fi
            OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}"libpcap from ${withval}|"
            RPM_CONFIG_FLAGS="${RPM_CONFIG_FLAGS} --with-libpcap=${withval}"
        fi
        if test "x$target_os" = "xcygwin"; then
            AC_CHECK_LIB(wpcap,pcap_open_offline, ,
                [AC_MSG_ERROR([cannot locate required WPCAP library])])
        else
            AC_CHECK_LIB(pcap,pcap_open_offline, ,
                [AC_MSG_ERROR([cannot locate required PCAP library])])
        fi
        AC_CHECK_HEADERS(pcap.h,[],
                [AC_MSG_ERROR([cannot locate required pcap.h header file])])
    fi
    ],[
    if test "x$target_os" = "xcygwin"; then
        AC_CHECK_LIB(wpcap, pcap_open_offline, ,
            [AC_MSG_ERROR([cannot locate required WPCAP library, try the --with-libpcap option])])
    else
        AC_CHECK_LIB(pcap, pcap_open_offline, ,
            [AC_MSG_ERROR([cannot locate required PCAP library, try the --with-libpcap option])])
    fi
    
    AC_CHECK_HEADERS(pcap.h,[],
        [AC_MSG_ERROR([cannot locate the required pcap.h header file])])
    ]
)


dnl ----------------------------------------------------------------------
dnl Check for libdag
dnl ----------------------------------------------------------------------

AC_ARG_WITH(dag,
AS_HELP_STRING([--with-dag=prefix],[include Endace DAG support]),[
if test "x$withval" != "xno"; then

  if test -d ${withval}; then
    LDFLAGS="-L${withval}/lib ${LDFLAGS}"
    CFLAGS="-I${withval}/include ${CFLAGS}"
    RPM_CONFIG_FLAGS="${RPM_CONFIG_FLAGS} --with-dag=${withval}"
    OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}"dag library ${withval}|"
  fi

    dnl look for libdag
    AC_CHECK_LIB(dag, dag_open,[
        AC_DEFINE(YAF_ENABLE_DAG, 1, [Define to 1 to enable DAG support])
        AC_SUBST(YAF_HAS_DAG, [1])
        LIBS="-ldag ${LIBS}"
        RPM_CONFIG_FLAGS="${RPM_CONFIG_FLAGS} --with-dag"
        OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}"dag library ${LIBS}|"
    ],[
        AC_MSG_ERROR([--with-dag given but cannot find libdag])
    ])

    AC_CHECK_HEADER(dagapi.h,,[
        AC_MSG_ERROR([--with-dag given but cannot find dagapi.h])
    ])
fi
])

dnl ----------------------------------------------------------------------
dnl Check for libpcapexpress
dnl ----------------------------------------------------------------------
AC_ARG_WITH(napatech,
    AS_HELP_STRING([--with-napatech=PCAPX_DIR],[include Napatech support (find pcapexpress.h in PCAPX_DIR/include, libpcapexpress in PCAPX_DIR/lib)]),[
    if test "x$withval" != "xno"; then
      if test -d ${withval}; then
        LDFLAGS="-L${withval}/lib ${LDFLAGS}"
        CFLAGS="-I${withval}/include ${CFLAGS} -pthread"
	CPPFLAGS="-I$withval/include ${CPPFLAGS}"
	LIBS="-L$withval/lib -pthread -lntcommoninterface $LIBS"
        RPM_CONFIG_FLAGS="${RPM_CONFIG_FLAGS} --with-napatech=${withval}"
        OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}" pcapexpress library ${withval}|"
      fi
    AC_CHECK_LIB([pcapexpress],[pcapexpress_open],[
       AC_DEFINE(YAF_ENABLE_NAPATECH, 1, [Define to 1 to enable Napatech support])
       AC_SUBST(YAF_HAS_NAPATECH, [1])
       LIBS="-lpcapexpress -lntcommoninterface -pthread ${LIBS}"
       RPM_CONFIG_FLAGS="${RPM_CONFIG_FLAGS} --with-napatech"
       OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}" pcapexpress (napatech) library ${LIBS}|"
     ],[
       AC_MSG_ERROR([--with-napatech given but cannot find libpcapexpress.  May need to set LDFLAGS to napatech library (libntcommoninterface)])
     ])
     AC_CHECK_HEADER(pcapexpress.h,,[
       AC_MSG_ERROR([--with-napatech given but cannot find pcapexpress.h])
     ])
   fi
   ]
)

dnl ----------------------------------------------------------------------
dnl Check for pcap-zcopy.h
dnl ----------------------------------------------------------------------
AC_ARG_WITH(bivio,
AS_HELP_STRING([--with-bivio=PCAP_ZCOPY_DIR],[include Bivio support (find pcap-zcopy.h in PCAP_ZCOPY_DIR/include)]),[
    if test "x$withval" != "xno"; then
      if test -d ${withval}; then
        CFLAGS="-I${withval}/include ${CFLAGS}"
      fi
     AC_CHECK_HEADERS(pcap-zcopy.h,[
       AC_DEFINE(YAF_ENABLE_BIVIO,1,[Define to 1 to enable BIVIO support])
        RPM_CONFIG_FLAGS="${RPM_CONFIG_FLAGS} --with-bivio=${withval}"
        OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}" bivio ${withval}|"
       ],[
       AC_MSG_ERROR([--with-bivio given but cannot find pcap-zcopy.h])
     ],
     [#ifdef HAVE_PCAP_H
      #include <pcap.h>
      #endif
     ])
   fi
])

dnl ----------------------------------------------------------------------
dnl Enable optional flow table features
dnl ----------------------------------------------------------------------

AC_ARG_ENABLE(compact-ip4,
AS_HELP_STRING([--disable-compact-ip4],
               [Use full address structure in flow table]),[
if test "x$enableval" = "xno"; then
    AC_MSG_NOTICE([Disabling compact IPv4 addresses in flow table])
    AC_DEFINE(YAF_ENABLE_COMPACT_IP4, 0,
              [Define to 1 to compact IPv4 addresses where possible])
    OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}"not-compact IPv4|"
    RPM_CONFIG_FLAGS="${RPM_CONFIG_FLAGS} --disable-compact-ip4"
else
    AC_DEFINE(YAF_ENABLE_COMPACT_IP4, 1,
              [Define to 1 to compact IPv4 addresses where possible])
    OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}"compact IPv4|"
fi
],[
    AC_DEFINE(YAF_ENABLE_COMPACT_IP4, 1,
              [Define to 1 to compact IPv4 addresses where possible])
    OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}"compact IPv4|"
])

dnl ----------------------------------------------------------------------
dnl Check for getaddrinfo
dnl ----------------------------------------------------------------------
AC_CHECK_FUNCS(getaddrinfo)

dnl ----------------------------------------------------------------------
dnl see if the user wants to enable hooks/plguins
dnl ----------------------------------------------------------------------
AC_ARG_ENABLE(plugins,
	AC_HELP_STRING([--enable-plugins], [enable YAF to load plugin extensions (default is no)]),
        [
      AC_DEFINE(YAF_ENABLE_HOOKS, 1, [Define to 1 to enable plugin support])
                plugins=true
		OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}"Plugin Support |"
                RPM_CONFIG_FLAGS="${RPM_CONFIG_FLAGS} --enable-plugins"
                AC_SUBST(YAF_REQ_PLUG_CONF, [1])
        ],
        [
                plugins=false
        ]
)
AM_CONDITIONAL([PLUGINENABLE], [test x$plugins = xtrue])

dnl ----------------------------------------------------------------------
dnl check if the user wants to enable export of DNS Authoritative Resp ONLY
dnl ----------------------------------------------------------------------
AC_ARG_ENABLE(exportDNSAuth,
	AC_HELP_STRING([--enable-exportDNSAuth], [enable export of DNS Authoritative Responses Only (default is export everything)]),
	[
		if test "x$enableval" = "xno"; then
		   AC_MSG_NOTICE([Enabling export of all DNS Responses])
		   exportDNSAuth = false
		else
		AC_DEFINE(YAF_ENABLE_DNSAUTH, 1, [Define to 1 to enable export of DNS Authoritative Responses Only])
		exportDNSAuth=true
		if test "x$plugins" != "xtrue"; then
		   AC_MSG_ERROR([DNS Authoritative Response Export requires plugins to be enabled])
		fi
		if test "x$applabeler" != "xtrue"; then
		   AC_MSG_ERROR([Application labeling must be enabled to export DNS Authoritative Responses])
		fi
		   AC_MSG_NOTICE([Export only DNS Authoritative Responses is enabled])
          fi
	],
	[
	 exportDNSAuth=false
	 AC_MSG_NOTICE([Enable Export of DNS Authoritative Responses is disabled by default])
	]

)
AM_CONDITIONAL([EXPORTDNSAUTH], [test x$exportDNSAuth = xtrue])

dnl ----------------------------------------------------------------------
dnl check if the user wants to enable export of DNS NXDOMAIN Responses ONLY
dnl ----------------------------------------------------------------------
AC_ARG_ENABLE(exportDNSNXDomain,
        AC_HELP_STRING([--enable-exportDNSNXDomain], [enable export of DNS NXDomain Responses Only (default is export everything)]),
        [
                if test "x$enableval" = "xno"; then
                   AC_MSG_NOTICE([Enabling export of all DNS Responses])
                   exportDNSNXDomain = false
                else
                AC_DEFINE(YAF_ENABLE_DNSNXDOMAIN, 1, [Define to 1 to enable export of DNS NXDomain Responses Only])
                exportDNSNXDomain=true
                if test "x$plugins" != "xtrue"; then
                   AC_MSG_ERROR([DNS NXDomain Response Export requires plugins to be enabled])
                fi
                if test "x$applabeler" != "xtrue"; then
                   AC_MSG_ERROR([Application labeling must be enabled to export DNS Authoritative Responses])
                fi
                   AC_MSG_NOTICE([Export only DNS NXDomain Responses is enabled])
          fi
        ],
        [
         exportDNSNXDomain=false
         AC_MSG_NOTICE([Enable Export of DNS NXDomain Responses is disabled by default])
        ]

)
AM_CONDITIONAL([EXPORTDNSNXDomain], [test x$exportDNSNXDomain = xtrue])


dnl ----------------------------------------------------------------------
dnl check if the user wants to enable the DAG interface hack
dnl ----------------------------------------------------------------------
AC_ARG_ENABLE(daginterface,
    AC_HELP_STRING([--enable-daginterface], [enable encoding DAG interface numbers into the record output]),
    [
        if test "x$enableval" = "xno"; then
            AC_MSG_NOTICE([Disabling the DAG interface numbers recording])
            daginterfacehack=false
        else
            AC_DEFINE(YAF_ENABLE_DAG_SEPARATE_INTERFACES, 1, [Define to 1 to enable recording DAG interface numbers])
            daginterfacehack=true
            AC_MSG_NOTICE([DAG interface recording into output records is enabled])
            RPM_CONFIG_FLAGS="${RPM_CONFIG_FLAGS} --enable-daginterface"
            OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}"DAG interface labeling|"
        fi
    ],
    [
        AC_MSG_NOTICE([DAG interface recording into output record disabled by default])
        daginterfacehack=false
    ]
)

dnl ----------------------------------------------------------------------
dnl check if the user wants to enable the Napatech interface hack
dnl ----------------------------------------------------------------------
AC_ARG_ENABLE(napatechinterface,
    AC_HELP_STRING([--enable-napatechinterface], [enable encoding Napatech interface numbers into the record output]),
    [
        if test "x$enableval" = "xno"; then
            AC_MSG_NOTICE([Disabling the Napatech interface numbers recording])
            napatechinterface=false
        else
            AC_DEFINE(YAF_ENABLE_NAPATECH_SEPARATE_INTERFACES, 1, [Define to 1 to enable recording DAG interface numbers])
            napatechinterface=true
            AC_MSG_NOTICE([Napatech interface recording into output records is enabled])
            RPM_CONFIG_FLAGS="${RPM_CONFIG_FLAGS} --enable-napatechinterface"
            OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}"Napatech interface labeling|"
        fi
    ],
    [
        AC_MSG_NOTICE([Napatech interface recording into output record disabled by default])
        napatechinterface=false
    ]
)

dnl ----------------------------------------------------------------------
dnl Check for Spread Support
dnl ----------------------------------------------------------------------
m4_include([autoconf/spread.m4])
AC_PATH_SPREAD([4.1])
if test "x$ac_cv_header_spread_sp_h" == "xyes"; then
   AC_SUBST(FIXBUF_REQ_LIBSPREAD, [1])
fi

dnl ----------------------------------------------------------------------
dnl Check for Unix vs. Linux privdrop (EWW EWW HACK EWW)
dnl ----------------------------------------------------------------------
if test "x$target_os" = "xlinux" -o \
        "x$target_os" = "xlinux-gnu"; then
    AC_MSG_NOTICE([Enabling Linux privilege drop hack])
    AC_DEFINE([LINUX_PRIVHACK], 1,
              [Define to 1 on Linux for privilege drop hack])
else
    AC_MSG_NOTICE([Normal privilege drop (target os is $target_os)])
fi


dnl ----------------------------------------------------------------------
dnl Check if this is Solaris and enabled the impure-text option
dnl ----------------------------------------------------------------------
case $target_os in
	solaris*) LDFLAGS="$LDFLAGS -mimpure-text" ;;
esac

dnl ----------------------------------------------------------------------
dnl Add a conditional flag into Automake for Cygwin checking
dnl ----------------------------------------------------------------------
AM_CONDITIONAL([CYGWIN], [test "x$target_os" = "xcygwin"])

dnl ----------------------------------------------------------------------
dnl Lets put the configuation information into the version output for YAF
dnl ----------------------------------------------------------------------
OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}"\n"
AC_DEFINE_UNQUOTED([YAF_ACONF_STRING_STR], ["${OPTION_CONFIG_STRING}"], [configure script options])


dnl ----------------------------------------------------------------------
dnl Write Makefiles (and all the other files too)
dnl ----------------------------------------------------------------------
AC_SUBST(LIBCOMPAT)
AC_SUBST(RPM_CONFIG_FLAGS)


dnl automake 1.9 doesn't substitute these automatically, so we do it ourselves
dnl just in case
AC_SUBST([abs_top_srcdir])
AC_SUBST([abs_top_builddir])

AC_CONFIG_FILES([Makefile
				src/Makefile
				include/Makefile
				etc/Makefile
				src/yaf.pod
				libyaf.pc
				etc/init.d/yaf
				yaf.spec
				Doxyfile
				README
				etc/yaf.conf
				airframe/Makefile
				airframe/src/Makefile
				airframe/include/Makefile
				airframe/doc/Makefile
				airframe/libairframe.pc
				airframe/libairframe.spec
				airframe/Doxyfile
				])

AC_OUTPUT
