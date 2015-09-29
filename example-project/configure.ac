AC_INIT([youtils], [0.1.0])
AC_CONFIG_SRCDIR([./example.cpp])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([.])
AM_INIT_AUTOMAKE([foreign])
LT_INIT(disable-shared)
AC_ARG_WITH(pkgconfigdir,
            [  --with-pkgconfigdir=DIR      pkgconfig file in DIR @<:@LIBDIR/pkgconfig@:>@],
            [pkgconfigdir=$withval],
            [pkgconfigdir='${libdir}/pkgconfig'])
AC_SUBST(pkgconfigdir)
PKG_CHECK_MODULES([BUILDTOOLS], [buildtools])
AC_ARG_VAR([VOLUMEDRIVER_BUILD_DIR], [place where the volumedriver was built])
AC_ARG_VAR([VOLUMEDRIVER_DIR], [directory that has the volumedriver sources])
AC_ARG_VAR([VOLUMEDRIVER_DATA_DIR], [place to put the data concerning for volumedriver])
dnl AC_CONFIG_SUBDIRS([test])
AC_PROG_CC
AC_PROG_CXX
AC_CONFIG_FILES([Makefile volumedriver.json])
AC_OUTPUT
