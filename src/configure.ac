AC_INIT([volumedriver_project], [0.1.0])
AC_CONFIG_SRCDIR([youtils/Assert.h])
AC_CONFIG_MACRO_DIR([m4])
m4_include([m4/vd_macros.m4])
AC_CONFIG_AUX_DIR([.])
AM_INIT_AUTOMAKE(m4_esyscmd([case `automake --version | head -n 1` in
                             *1.11*) echo "subdir-objects foreign";;
                             *) echo "subdir-objects foreign serial-tests";;
                             esac]))
dnl AM_INIT_AUTOMAKE([foreign])
AC_ARG_VAR(BUILDTOOLS, [set the location of buildtools])
BUILDTOOLS_DIR()
AC_CONFIG_SUBDIRS([scripts
		   youtils
		   fawltyfs
		   simple-nfs
		   backend
		   backend-python
		   xmlrpc++0.7
		   VolumeTester
		   python-base
		   volumedriver
		   filedriver
		   filesystem
		   filesystem-python-client
		   ganesha-filesystem
		   packaging])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([Doxyfile])
AC_OUTPUT
