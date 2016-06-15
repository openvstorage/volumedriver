AC_INIT([volumedriverfs],
	[0.1],
	[bugs@openvstorage.com],
	[volumedriverfs],
	[http://www.openvstorage.com])
dnl avoid the dreaded "configure.ac:9: required file `../ltmain.sh' not found" error
AC_CONFIG_SRCDIR([./FileSystem.cpp])
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
OMNIIDL()
VD_SCRIPT_DIR()
PROTOC()
LTTNG_GEN_TP()

AC_PROG_CC
AC_PROG_CXX
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([volumedriverfs.pc])
AC_CONFIG_FILES([.gdbinit])

AC_CONFIG_FILES([test/Makefile])
AC_CONFIG_FILES([test/event_message_test.py])
AC_CONFIG_FILES([volumedriver_fs.sh], [chmod +x volumedriver_fs.sh])
AC_CONFIG_FILES([test/volumedriver_fs_test.sh], [chmod +x test/volumedriver_fs_test.sh])
AC_CONFIG_FILES([test/python_test.sh], [chmod +x test/python_test.sh])

AC_CONFIG_FILES([failovercache/Makefile])

AC_CONFIG_FILES([failovercache/test/Makefile])
AC_CONFIG_FILES([c-api/Makefile])
AC_OUTPUT
