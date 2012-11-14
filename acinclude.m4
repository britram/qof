#
# Synopsys
#  Utility configuration checks for YAF.
#
# Description
#	Some more portability tests for running YAF on different platforms.
#
# Copyright (C) 2008-11 Carnegie Mellon University. All rights Reserved.
#
# GNU General Public License (GPL) Rights pursuant to Version 2, June 1991
# Government Purpose License Rights (GPLR) pursuant to DFARS 252.227-7013
#
# Developed as part of the YAF suite, CMU SEI CERT program, Network
# Situational Awareness group.
# http://www.cert.org
#
# mailto:netsa-help@cert.org
#
#


# YF_SIZE_T_FORMAT
#
# This tests the size of size_t and creates some handy macros
# for outputting the value of size_t variables without warnings
# across platforms
#
# creates #defines:
#   SIZE_T_FORMAT regular (f)print format for unsigned value
#   SIZE_T_FORMATX regular (f)print format for value in hex
#	SIZE_T_CAST a cast to be able to cast size_t's into a standard
#               formatter type (uint??_t) that is the same size as
#               a size_t
#
AC_DEFUN([YF_SIZE_T_FORMAT],[
	
	AC_MSG_CHECKING([for size of size_t])
	
	for bitSize in "8" "16" "32" "64"
	do
		AC_RUN_IFELSE([
			AC_LANG_PROGRAM([
				#if HAVE_STDDEF_H
				#include <stddef.h>
				#endif
				#if HAVE_LIMITS_H
				#include <limits.h>
				#endif
			],[
				if (sizeof(size_t)*CHAR_BIT == $bitSize) return 0;
				return 1;
			])
		],[SIZE_T_SIZE=$bitSize])
	done

	AC_MSG_RESULT([$SIZE_T_SIZE])

	case $SIZE_T_SIZE in
		8 )
			AC_DEFINE(SIZE_T_FORMAT,[PRIu8],[(f)printf format string for type size_t])
			AC_DEFINE(SIZE_T_FORMATX,[PRIx8],[(f)printf format string for type size_t])
			AC_DEFINE(SIZE_T_CAST,[uint8_t],[size_t cast for string formatting])
			;;
		16 )
			AC_DEFINE(SIZE_T_FORMAT,[PRIu16],[(f)printf format string for type size_t])
			AC_DEFINE(SIZE_T_FORMATX,[PRIx16],[(f)printf format string for type size_t])
			AC_DEFINE(SIZE_T_CAST,[uint16_t],[size_t cast for string formatting])
			;;
		32 )
			AC_DEFINE(SIZE_T_FORMAT,[PRIu32],[(f)printf format string for type size_t])
			AC_DEFINE(SIZE_T_FORMATX,[PRIx32],[(f)printf format string for type size_t])
			AC_DEFINE(SIZE_T_CAST,[uint32_t],[size_t cast for string formatting])
			;;
		64 )
			AC_DEFINE(SIZE_T_FORMAT,[PRIu64],[(f)printf format string for type size_t])
			AC_DEFINE(SIZE_T_FORMATX,[PRIx64],[(f)printf format string for type size_t])
			AC_DEFINE(SIZE_T_CAST,[uint64_t],[size_t cast for string formatting])
			;;
	esac
	
])

#
# YF_PKGCONFIG_VERSION
#
# This returns the version number of the tool found for the provided
# library.  
#
# YF_PKGCONFIG_VERSION(library)
# output in yfpkg_ver
#
AC_DEFUN([YF_PKGCONFIG_VERSION],[
	AC_REQUIRE([PKG_PROG_PKG_CONFIG])
	yfpkg_ver=`$PKG_CONFIG --modversion $1`
])

#
# YF_PKGCONFIG_LPATH
#
# This returns the library path (or at least the first one returned from
# pkg-config) for the provided library.
#
#
# YF_PKGCONFIG_LPATH(library)
# output in yfpkg_lpath
#
AC_DEFUN([YF_PKGCONFIG_LPATH],[
	AC_REQUIRE([PKG_PROG_PKG_CONFIG])
	yfpkg_lpath=`$PKG_CONFIG --libs-only-L $1 | cut -d' ' -f 1`
])


#
# YF_LIBSTR_STRIP
#
# strips a gcc/ld switch string string from something like
# "-L/usr/local/foo/lib/ -lblah" to just capture the first
# path "/usr/local/foo/lib" assuming the string is formatted
# just like shown
#
# FIXME
#
# YF_LIBSTR_STRIP("ld_option_string")
# output in yf_libstr
#
AC_DEFUN([YF_LIBSTR_STRIP],[
#	_resultString=[`echo $1 | sed 's/-L\([^ ]*\).*/\1/pg'`]
#	yf_libstr=${_resultString}
	yf_libstr=$1
])

