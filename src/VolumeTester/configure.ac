AC_INIT([VolumeTester], [0.1.0])
AC_CONFIG_SRCDIR([./DeviceReader.cpp])
AC_CONFIG_MACRO_DIR([../m4])
AC_CONFIG_AUX_DIR([..])
AM_INIT_AUTOMAKE(m4_esyscmd([case `automake --version | head -n 1` in
                             *1.11*) echo "subdir-objects foreign";;
                             *) echo "subdir-objects foreign serial-tests";;
                             esac]))
dnl AM_INIT_AUTOMAKE([foreign])
LT_INIT()
m4_include([../m4/vd_macros.m4])
VD_BUILDTOOLS()
VD_SCRIPT_DIR()

AM_PATH_PYTHON([2.6])
AC_PROG_CC
AC_PROG_CXX
AC_CONFIG_FILES([Makefile .gdbinit])
AC_CONFIG_FILES([pylint_test.sh], [chmod +x pylint_test.sh])
AC_OUTPUT
