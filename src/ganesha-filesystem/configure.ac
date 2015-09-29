AC_INIT([ganesha-filesystem],
	[0.0.0],
	[bugs@openvstorage.com],
	[ganesha-filesystem],
	[http://www.openvstorage.com])

AC_CONFIG_SRCDIR([./FileSystemWrapper.cpp])
AC_CONFIG_MACRO_DIR([../m4])
AC_CONFIG_AUX_DIR([../])
AM_INIT_AUTOMAKE(m4_esyscmd([case `automake --version | head -n 1` in
                             *1.11*) echo foreign;;
                             *) echo "foreign serial-tests";;
                             esac]))
dnl AM_INIT_AUTOMAKE([foreign])
LT_INIT(disable-static)
m4_include([../m4/vd_macros.m4])
VD_BUILDTOOLS()
BUILDTOOLS_DIR()
GANESHA_DIR()
VD_SCRIPT_DIR()
LTTNG_GEN_TP()

AC_PROG_CC
AC_PROG_CXX
dnl AC_CONFIG_SUBDIRS([test])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([.gdbinit])
AC_CONFIG_FILES([ganesha-cf], [chmod +x ganesha-cf])
AC_CONFIG_FILES([ganesha-cf.conf])

AC_CONFIG_FILES([test/Makefile])
AC_CONFIG_FILES([test/volumedriverfs_ganesha_test.sh], [chmod +x test/volumedriverfs_ganesha_test.sh])
AC_CONFIG_FILES([test/.gdbinit])

# Get rid of -lgcc_s automagically added to the linker flags
postdeps_CXX=`echo " $postdeps_CXX " | sed 's, -lgcc_s, -lgcc_eh,g'`
AC_OUTPUT
