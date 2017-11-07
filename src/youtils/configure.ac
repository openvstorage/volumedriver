AC_INIT([youtils], [0.1.0])
AC_CONFIG_SRCDIR([./Assert.h])
AC_CONFIG_MACRO_DIR([../m4])
AC_CONFIG_AUX_DIR([..])
AM_INIT_AUTOMAKE(m4_esyscmd([case `automake --version | head -n 1` in
                             *1.11*) echo "subdir-objects foreign";;
                             *) echo "subdir-objects foreign serial-tests";;
                             esac]))
dnl AM_INIT_AUTOMAKE([foreign serial-test])
LT_INIT()

m4_include([../m4/vd_macros.m4])
DETECT_CLANGPP()
VD_BUILDTOOLS()
VD_SCRIPT_DIR()
OMNIIDL()
dnl AC_CONFIG_SUBDIRS([test corbatest])
AC_PROG_CC
AC_PROG_CXX
AC_CONFIG_FILES([youtils.pc])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([.gdbinit])
AC_CONFIG_FILES([test/Makefile])
AC_CONFIG_FILES([test/logging_speed_test.sh], [chmod +x test/logging_speed_test.sh])
AC_CONFIG_FILES([test/youtils_test.sh], [chmod +x test/youtils_test.sh])
AC_CONFIG_FILES([buildinfo.sh], [chmod +x buildinfo.sh])
AC_CONFIG_FILES([corbatest/Makefile])
AC_CONFIG_FILES([corbatest/.gdbinit])
#AC_CONFIG_FILES([compilerlab/Makefile])
dnl AC_CONFIG_FILES([compilerlab/CompilerLab.cpp])
#AC_CONFIG_FILES([compilerlab/runcompilerlab.sh], [chmod +x compilerlab/runruncompilerlab.sh])
AC_CONFIG_FILES([prudence/Makefile])
AC_CONFIG_FILES([prudence/prudence_test.sh], [chmod +x prudence/prudence_test.sh])
AC_CONFIG_FILES([prudence/.gdbinit])
# Get rid of -lgcc_s automagically added to the linker flags
postdeps_CXX=`echo " $postdeps_CXX " | sed 's, -lgcc_s, -lgcc_eh,g'`

AC_OUTPUT
