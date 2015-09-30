AC_DEFUN([VD_BUILDTOOLS], [
AC_ARG_WITH(pkgconfigdir,
            [  --with-pkgconfigdir=DIR      pkgconfig file in DIR @<:@LIBDIR/pkgconfig@:>@],
            [pkgconfigdir=$withval],
            [pkgconfigdir='${libdir}/pkgconfig'])
AC_SUBST(pkgconfigdir)
PKG_CHECK_MODULES([BUILDTOOLS], [buildtools = 5.0])
])

AC_DEFUN([OMNIIDL], [
	AC_ARG_WITH(omniidl,
	[--with-omniidl=<path to omniidl> ],
	[omniidl=$withval])
AC_SUBST(omniidl)
])

AC_DEFUN([PROTOC], [
	AC_ARG_WITH(protoc,
	[--with-protoc=<path to protoc> ],
	[protoc=$withval])
AC_SUBST(protoc)
])

AC_DEFUN([CAPNPC], [
         AC_ARG_WITH(capnpc,
	 [--with-capnpc=<path to capnpc> ],
	 [capnpc=$withval])
AC_SUBST(capnpc)
])

AC_DEFUN([FIO], [
	AC_ARG_WITH(fio,
	[--with-fio=<path to fio> ],
	[fio=$withval])
AC_SUBST(fio)
])

AC_DEFUN([LTTNG_GEN_TP], [
	AC_ARG_WITH(lttng_gen_tp,
	[--with-lttng-gen-tp=<path to lttng-gen-tp> ],
	[lttng_gen_tp=$withval])
AC_SUBST(lttng_gen_tp)
])

AC_DEFUN([VD_SCRIPT_DIR], [
AC_SUBST([script_directory])
script_directory=${prefix}/share/volumedriverscripts
])

AC_DEFUN([VD_FIO_PLUGIN_DIR], [
	AC_SUBST([fio_plugin_dir])
fio_plugin_dir=${prefix}/lib/fio-plugins
])

AC_DEFUN([GANESHA_DIR], [
	AC_ARG_WITH(ganesha,
	[--with-ganesha=<path to ganesha includes> ],
	[ganesha=$withval])
AC_SUBST(ganesha)
])

AC_DEFUN([BUILDTOOLS_DIR], [
	AC_ARG_WITH(buildtoolsdir,
	[--with-buildtoolsdir=<path to buildtools to use> ],
	[buildtoolsdir=$withval])
AC_SUBST(buildtoolsdir)
])

AC_DEFUN([DETECT_CLANGPP], [
	AC_CACHE_CHECK([whether we're using clang++],
		[vd_cv_use_clangpp], [
		vd_cv_use_clangpp=no
		if test $(${CXX} -v 2>&1 | grep -c clang) -eq 1; then vd_cv_use_clangpp=yes; fi ])
	AM_CONDITIONAL([CLANGPP], [test $vd_cv_use_clangpp = yes ])
])
