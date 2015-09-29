AC_INIT([simple-nfs],
	[0.1],
	[bugs@openvstorage.com],
	[simple-nfs],
	[http://www.openvstorage.com])
dnl avoid the dreaded "configure.ac:9: required file `../ltmain.sh' not found" error
AC_CONFIG_SRCDIR([./Mount.h])
AC_CONFIG_MACRO_DIR([../m4])
AC_CONFIG_AUX_DIR([..])
AM_INIT_AUTOMAKE(m4_esyscmd([case `automake --version | head -n 1` in
                             *1.11*) echo foreign;;
                             *) echo "foreign serial-tests";;
                             esac]))
dnl AM_INIT_AUTOMAKE([foreign])
AM_PATH_PYTHON([2.6])
LT_INIT()
m4_include([../m4/vd_macros.m4])
VD_BUILDTOOLS()
VD_SCRIPT_DIR()

AC_PROG_CC
AC_PROG_CXX
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([simplenfs.pc])

# Get rid of -lgcc_s automagically added to the linker flags
postdeps_CXX=`echo " $postdeps_CXX " | sed 's, -lgcc_s, -lgcc_eh,g'`

AC_OUTPUT
