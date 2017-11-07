AC_INIT([filedriver],[0.0.1])
AC_CONFIG_SRCDIR([./ContainerManager.h])
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
dnl AC_CONFIG_SUBDIRS([test])
AC_PROG_CC
AC_PROG_CXX
AC_CONFIG_FILES([filedriver.pc])
AC_CONFIG_FILES([.gdbinit])
AC_CONFIG_FILES([Makefile])

AC_CONFIG_FILES([test/filedriver_tests.sh], [chmod +x test/filedriver_tests.sh])
AC_CONFIG_FILES([test/Makefile])
AC_OUTPUT
