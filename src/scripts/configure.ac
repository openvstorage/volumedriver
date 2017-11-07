AC_INIT([scripts], [0.1.0])
AC_CONFIG_SRCDIR([./catch_errors_maybe_generate_xml.sh.in])
AC_CONFIG_MACRO_DIR([../m4])
AC_CONFIG_AUX_DIR([..])
m4_include([../m4/vd_macros.m4])
m4_include([../m4/ax_python.m4])
VD_SCRIPT_DIR()
AM_INIT_AUTOMAKE(m4_esyscmd([case `automake --version | head -n 1` in
                             *1.11*) echo "subdir-objects foreign";;
                             *) echo "subdir-objects foreign serial-tests";;
                             esac]))
dnl AM_INIT_AUTOMAKE([foreign])
AX_PYTHON()
AM_PATH_PYTHON([2.6])
AC_CONFIG_FILES([Makefile pylint_test.sh cpp_test.sh python_test.sh postprocess_gtest_xml.py])
AC_CONFIG_FILES([catch_errors_maybe_generate_xml.sh])
AC_OUTPUT
