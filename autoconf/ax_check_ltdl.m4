#
# Check to see if libltdl is already installed, if so, use that version,
# if not, use a convenience version.  
#
# 
#

AC_DEFUN([USE_LTDL],[
	AC_LIBLTDL_CONVENIENCE
])
	

AC_DEFUN([AC_PATH_LIBLTDL], [
  
        LIBLTDLPRESENT=YES

	AC_CHECK_HEADER([ltdl.h],
	[AC_CHECK_LIB([ltdl], [lt_dladvise_init],
	[LIBLTDL=-lltdl], [LIBLTDLPRESENT=NO])],
	[LIBLTDLPRESENT=NO])
	
	if test x"$LIBLTDLPRESENT" == "xNO"; then
	   AC_MSG_NOTICE([system libltdl not installed. building convenience libltdl])
	   USE_LTDL
	else
	   AC_MSG_NOTICE([using system libltdl])
	fi

])

