AC_INIT([fawltyfs], [0.1.0])
AC_CONFIG_SRCDIR([./DirectIORule.h])
AC_CONFIG_MACRO_DIR([../m4])
AC_CONFIG_AUX_DIR([..])
m4_include([../m4/vd_macros.m4])
VD_BUILDTOOLS()
OMNIIDL()
VD_SCRIPT_DIR()
AM_INIT_AUTOMAKE(m4_esyscmd([case `automake --version | head -n 1` in
                             *1.11*) echo foreign;;
                             *) echo "foreign serial-tests";;
                             esac]))
dnl AM_INIT_AUTOMAKE([foreign])
LT_INIT()
dnl AC_CONFIG_SUBDIRS([pyfawltyfs corba-daemon test])
AM_PATH_PYTHON([2.6])
AC_PROG_CC
AC_PROG_CXX
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([pyfawltyfs/Makefile])
AC_CONFIG_FILES([pyfawltyfs/__init__.py])
AC_CONFIG_FILES([pyfawltyfs/.gdbinit])
AC_CONFIG_FILES([pyfawltyfs/python_init])
AC_CONFIG_FILES([pyfawltyfs/abort-fuse.sh], [chmod -f +x pyfawltyfs/abort-fuse.sh])
AC_CONFIG_FILES([pyfawltyfs/runpython.sh], [chmod -f +x pyfawltyfs/runpython.sh])


AC_CONFIG_FILES([pyfawltyfs/test/Makefile])
AC_CONFIG_FILES([pyfawltyfs/test/__init__.py])
AC_CONFIG_FILES([pyfawltyfs/test/TestBasicTests.py])
AC_CONFIG_FILES([pyfawltyfs/test/pylint_test.sh], [chmod +x pyfawltyfs/test/pylint_test.sh])
AC_CONFIG_FILES([pyfawltyfs/test/python_test.sh], [chmod +x pyfawltyfs/test/python_test.sh])

AC_CONFIG_FILES([corba-daemon/Makefile])

AC_CONFIG_FILES([test/Makefile])
AC_CONFIG_FILES([test/fawltyfs_test.sh], [chmod +x test/fawltyfs_test.sh])

 # Get rid of -lgcc_s automagically added to the linker flags
postdeps_CXX=`echo " $postdeps_CXX " | sed 's, -lgcc_s, -lgcc_eh,g'`

AC_OUTPUT
