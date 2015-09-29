AC_INIT([debian_packaging], [0.1.0])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_MACRO_DIR([../../m4])
AM_INIT_AUTOMAKE(m4_esyscmd([case `automake --version | head -n 1` in
                             *1.11*) echo foreign;;
                             *) echo "foreign serial-tests";;
                             esac]))
dnl AM_INIT_AUTOMAKE([foreign])
AM_PATH_PYTHON([2.6])
AC_CONFIG_SUBDIRS([volumedriver-base volumedriver-pitreplication volumedriver-server volumedriver-test])
AC_CONFIG_FILES([Makefile add-installdir.sh])

AC_OUTPUT
