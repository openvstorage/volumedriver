AC_INIT([backend], [0.1.0])
AC_CONFIG_SRCDIR([./BackendConfig.cpp])
AC_CONFIG_MACRO_DIR([../m4])
AC_CONFIG_AUX_DIR([..])
AM_INIT_AUTOMAKE(m4_esyscmd([case `automake --version | head -n 1` in
                             *1.11*) echo foreign;;
                             *) echo "foreign serial-tests";;
                             esac]))
dnl AM_INIT_AUTOMAKE([foreign])
LT_INIT()
m4_include([../m4/vd_macros.m4])
VD_BUILDTOOLS()
VD_SCRIPT_DIR()
BUILDTOOLS_DIR()
LTTNG_GEN_TP()

AC_ARG_VAR(BUILDTOOLS, [set the location of buildtools])
dnl AC_CONFIG_SUBDIRS([test])
AC_PROG_CC
AC_PROG_CXX
AC_CONFIG_FILES([backend.pc])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([.gdbinit])
AC_CONFIG_FILES([delete-test-namespace.sh], [chmod +x delete-test-namespace.sh])
AC_CONFIG_FILES([test/backend_test.sh], [chmod +x test/backend_test.sh])
AC_CONFIG_FILES([test/Makefile])
AC_OUTPUT
