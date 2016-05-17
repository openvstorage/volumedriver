AC_INIT([packaging], [0.1.0])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_MACRO_DIR([../m4])
AM_INIT_AUTOMAKE(m4_esyscmd([case `automake --version | head -n 1` in
                             *1.11*) echo foreign;;
                             *) echo "foreign serial-tests";;
                             esac]))
dnl AM_INIT_AUTOMAKE([foreign])
AM_PATH_PYTHON([2.6])

is_ubuntu=$(grep -qi ubuntu /etc/os-release; echo $?)
AM_CONDITIONAL([UBUNTU], [test $is_ubuntu -eq 0])

AM_CONDITIONAL([REDHAT], [test -x /usr/bin/rpmbuild])

AC_CONFIG_SUBDIRS([debian rpm])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
