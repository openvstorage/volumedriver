AC_INIT([base],
	[4.0.0],
	[bugs@openvstorage.com],
	[http://www.openvstorage.com])

AC_CONFIG_SRCDIR([./__init__.py.in])
AC_CONFIG_MACRO_DIR([../m4])
AC_CONFIG_AUX_DIR([..])
AM_INIT_AUTOMAKE(m4_esyscmd([case `automake --version | head -n 1` in
                             *1.11*) echo foreign;;
                             *) echo "foreign serial-tests";;
                             esac]))
dnl AM_INIT_AUTOMAKE([foreign])
AM_PATH_PYTHON([2.6])
m4_include([../m4/vd_macros.m4])
VD_SCRIPT_DIR()
dnl AC_CONFIG_SUBDIRS([test])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([__init__.py])
AC_CONFIG_FILES([volumedriver/__init__.py])
AC_CONFIG_FILES([test/Makefile])
AC_CONFIG_FILES([test/pylint_test.sh], [chmod +x test/pylint_test.sh])
AC_CONFIG_FILES([test/python_test.sh], [chmod +x test/python_test.sh])
AC_OUTPUT
