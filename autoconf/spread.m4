# tests for Spread EMS headers and libraries
#
#

AC_DEFUN([PT_SPREAD_REQUIRED],[
    te_req_err="true"
    echo "WARNING: Specifying --with-$1=no or --without-$1 will prevent Spread"
    echo "         support from being built."
])

AC_DEFUN([PT_SPREAD_NOT_FOUND],[
    echo "WARNING: Spread $1 not found - YAF will be built without Spread support"
])

#
#   PT_TRY_HEADER( incpath )
#
#   look for the headers in incpath

AC_DEFUN([PT_TRY_HEADER],[
    if test "x$1" != "x"; then
        _te_save_cppflags=$CPPFLAGS
        CPPFLAGS="-I$1 $CPPFLAGS"
    fi
    AC_CHECK_HEADERS([sp.h],[te_header_found="true"])
    if test "x$1" != "x"; then
        CPPFLAGS=$_te_save_cppflags
        unset _te_save_cppflags
    fi
])

#
#   PT_TRY_LINK( incpath, libpath )
#
#   Try linking the spread library to validate inc and lib paths

AC_DEFUN([PT_TRY_LINK],[
    if test "x$1" != "x"; then
        _te_save_cppflags=$CPPFLAGS
        CPPFLAGS="-I$1 $CPPFLAGS"
    fi
    _te_save_libs=$LIBS
    if test "x$2" != "x"; then
        LIBS="-L$2 -l$te_lib_name $LIBS"
    else
        LIBS="-l$te_lib_name $LIBS"
    fi
    
    AC_LINK_IFELSE(
        [AC_LANG_PROGRAM(
            [[#include <sp.h> ]],
            [[  mailbox mbox;
                char    *pg = NULL;
                int res = SP_connect( "foo", "bar", 0, 1, &mbox, pg );
            ]])],
        [te_library_found=true]
        [])

    if test "x$1" != "x"; then
        CPPFLAGS=$_te_save_cppflags
        unset _te_save_cppflags
    fi
    LIBS=$_te_save_libs
    unset _te_save_libs
])

#
#   The only public macro
#   
#   AC_PATH_SPREAD( version )
#

AC_DEFUN([AC_PATH_SPREAD],[
    te_path=""
    te_install_path="no"
    te_inc_path=""
    te_header_found="false"
    te_lib_path=""
    te_library_found="false"
    te_req_err="false"
    te_lib_name="spread"
    te_pthreadlib=""
    te_pthread="yes"

    AC_ARG_WITH([spread],
        AC_HELP_STRING([--with-spread=DIR],[location of Spread]),
            [   case $withval in
                    yes) te_install_path="yes" ;;
                    no) PT_SPREAD_REQUIRED([spread]) ;;
                    *) te_install_path="yes"
                       te_path="$withval" ;;
                esac ], [te_req_err="true"])

    if test "$te_install_path" = "no"; then
        AC_ARG_WITH([spread-include],
            AC_HELP_STRING([--with-spread-include=DIR],[location of Spread headers]),
                [   case $withval in
                        yes) ;;
                        no) PT_SPREAD_REQUIRED([spread-include]) ;;
                        *) te_inc_path="$withval" ;;
                    esac ], [te_req_err="true"])

        AC_ARG_WITH([spread-lib],
            AC_HELP_STRING([--with-spread-lib=DIR],[location of Spread libraries]),
                [   case $withval in
                        yes) ;;
                        no) PT_SPREAD_REQUIRED([spread-lib]) ;;
                        *) te_lib_path="$withval" ;;
                    esac ], [te_req_err="true"])
    fi

    # ---- test for headers
    if test "x$te_req_err" = "xfalse"; then
    if test "x$te_inc_path" = "x"; then
        if test "x$te_path" = "x"; then
            AC_MSG_NOTICE(checking for sp.h in default locations)
        else
            AC_MSG_NOTICE(checking for sp.h in install directory $te_path)
            te_inc_path=${te_path}/include
        fi
    else
        AC_MSG_NOTICE(checking for sp.h in Spread include directory $te_inc_path)
    fi

    PT_TRY_HEADER([$te_inc_path])

    if test "$te_header_found" = "false"; then
        PT_SPREAD_NOT_FOUND([header],[include])
    fi

    # ---- test for libraries

    if test "x$te_lib_path" = "x"; then
        if test "x$te_path" = "x"; then
            AC_MSG_NOTICE(checking for $te_lib_name library in default locations)
        else
            te_lib_path=${te_path}/lib
            AC_MSG_NOTICE(checking for $te_lib_name library in install directory $te_path)
        fi
    else
        AC_MSG_NOTICE(checking for $te_lib_name library in Spread lib directory $te_lib_path)
    fi

    PT_TRY_LINK([$te_inc_path],[$te_lib_path])

    if test "$te_library_found" = "false"; then
        PT_SPREAD_NOT_FOUND([library],[lib])
    fi

    #--- everthing is good so far, check for pthread

    AC_CHECK_HEADERS([pthread.h],,[
        te_pthread="no"
        AC_MSG_WARN([pthread.h not found, Spread support will not be built])
    ])

    AC_CHECK_LIB([pthread],[pthread_mutex_lock],[te_pthreadlib=pthread],[
        te_pthread="no"
        AC_MSG_WARN([pthread library not found, Spread support will not be built])
    ])

    if test "$te_pthread" == "yes"; then
        if test "$te_library_found" == "true"; then
            if test "$te_header_found" == "true"; then
	        OPTION_CONFIG_STRING=${OPTION_CONFIG_STRING}"spread|"
                if test "x$te_inc_path" != "x"; then
                    AC_MSG_NOTICE(using sp.h found in $te_inc_path)
                    AC_SUBST([SPREAD_CFLAGS],["-I$te_inc_path -DHAVE_SPREAD"])
		else
		    AC_MSG_NOTICE(using sp.h found in default include path)
		    AC_SUBST([SPREAD_CFLAGS],["-DHAVE_SPREAD"])
                fi
                AC_SUBST([SPREAD_CC_DEFINE],["-DHAVE_SPREAD"])

                if test "x$te_lib_path" != "x"; then
                    AC_MSG_NOTICE(using libspread found in $te_lib_path)
                    AC_SUBST([SPREAD_LDFLAGS],[-L$te_lib_path])
		else
		    AC_MSG_NOTICE(using libspread found in default library path)
                fi
                if test "x$te_pthreadlib" != "x"; then
                    AC_SUBST([SPREAD_LIBS],["-l$te_lib_name -l$te_pthreadlib"])
                else
                    AC_SUBST([SPREAD_LIBS],["-l$te_lib_name"])
                fi

            fi
        fi
    fi
fi
    unset te_path
    unset te_install_path
    unset te_inc_path
    unset te_header_found
    unset te_lib_path
    unset te_library_found
    unset te_req_err
    unset te_pthreadlib
    unset te_do_spread
])

