AC_INIT([backend-python],
	[0.0.1],
	[bugs@openvstorage.com],
	[backend-python],
	[http://www.openvstorage.com])

AC_CONFIG_SRCDIR([./Backend.cpp])
AC_CONFIG_MACRO_DIR([../m4])
AC_CONFIG_AUX_DIR([../])
AM_INIT_AUTOMAKE(m4_esyscmd([case `automake --version | head -n 1` in
                             *1.11*) echo "subdir-objects foreign";;
                             *) echo "subdir-objects foreign serial-tests";;
                             esac]))
dnl AM_INIT_AUTOMAKE([foreign])
LT_INIT(disable-static)
m4_include([../m4/vd_macros.m4])
VD_BUILDTOOLS()
VD_SCRIPT_DIR()
AM_PATH_PYTHON([2.6])
AC_PROG_CC
AC_PROG_CXX
dnl AC_CONFIG_SUBDIRS([test])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([__init__.py])
AC_CONFIG_FILES([.gdbinit])
AC_CONFIG_FILES([runpython.sh], [chmod +x runpython.sh])
AC_CONFIG_FILES([bug/Makefile])

AC_CONFIG_FILES([test/Makefile])
AC_CONFIG_FILES([test/backend_python_test.py])
AC_CONFIG_FILES([test/python_test.sh], [chmod +x test/python_test.sh])

# Get rid of -lgcc_s automagically added to the linker flags
postdeps_CXX=`echo " $postdeps_CXX " | sed 's, -lgcc_s, -lgcc_eh,g'`

AC_OUTPUT
