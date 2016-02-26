AC_INIT([volumedriver-dev], [3.6.0])
AC_CONFIG_SRCDIR([./debian/rules.in])
AC_CONFIG_MACRO_DIR([../../../m4])
AC_CONFIG_AUX_DIR([../../..])
AM_INIT_AUTOMAKE(m4_esyscmd([case `automake --version | head -n 1` in
                             *1.11*) echo foreign;;
                             *) echo "foreign serial-tests";;
                             esac]))
dnl AM_INIT_AUTOMAKE([foreign])
AM_PATH_PYTHON([2.6])
AM_CONDITIONAL([USE_MD5_HASH], [test "x$USE_MD5_HASH" != xno])
m4_include([../../../m4/vd_macros.m4])
AC_SUBST([VD_MAJOR_VERSION])
AC_SUBST([VD_MINOR_VERSION])
AC_SUBST([VD_PATCH_VERSION])
AC_SUBST([VD_EXTRA_VERSION])
BUILDTOOLS_DIR()
AC_CONFIG_FILES([Makefile
		 debian/control-template
		 debian/control-template-no-dedup
		 debian/copyright
	  	 debian/source/format
		 debian/changelog
		 debian/rules
		 debian/compat
		 debian/volumedriver-dev.docs
		 debian/volumedriver-no-dedup-dev.docs
         debian/volumedriver-no-dedup-dev-head.install
		 debian/volumedriver-dev-head.install])
AC_OUTPUT

