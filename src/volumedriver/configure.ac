AC_INIT([volumedriver],	[3.4.0])
AC_CONFIG_SRCDIR([./Api.h])
AC_CONFIG_MACRO_DIR([../m4])
AC_CONFIG_AUX_DIR([..])
AM_INIT_AUTOMAKE(m4_esyscmd([case `automake --version | head -n 1` in
                             *1.11*) echo foreign;;
                             *) echo "foreign serial-tests";;
                             esac]))
dnl AM_INIT_AUTOMAKE([foreign])
LT_INIT()
m4_include([../m4/vd_macros.m4])

VD_BUILDTOOLS()
CAPNPC()
VD_SCRIPT_DIR()
PROTOC()
BUILDTOOLS_DIR()
LTTNG_GEN_TP()

AC_PROG_CC
AC_PROG_CXX
AM_PATH_PYTHON([2.6])

AC_CONFIG_FILES([volumedriver.pc])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([.gdbinit])

AC_CONFIG_FILES([test/Makefile])
AC_CONFIG_FILES([test/volumedriver_tests.sh], [chmod +x test/volumedriver_tests.sh])

AC_CONFIG_FILES([ToolCut/Makefile])
AC_CONFIG_FILES([ToolCut/__init__.py])
AC_CONFIG_FILES([ToolCut/.gdbinit])
AC_CONFIG_FILES([ToolCut/runpython.sh], [chmod +x ToolCut/runpython.sh])

AC_CONFIG_FILES([ToolCut/test/Makefile])
AC_CONFIG_FILES([ToolCut/test/toolcut_test.py])
AC_CONFIG_FILES([ToolCut/test/pylint_test.sh], [chmod +x ToolCut/test/pylint_test.sh])
AC_CONFIG_FILES([ToolCut/test/python_test.sh], [chmod +x ToolCut/test/python_test.sh])

AC_CONFIG_FILES([pitreplication/Makefile])
AC_CONFIG_FILES([pitreplication/__init__.py])
AC_CONFIG_FILES([pitreplication/pylint_test.sh], [chmod +x pitreplication/pylint_test.sh])

AC_CONFIG_FILES([metadata-server/Makefile])
AC_CONFIG_FILES([metadata-server/.gdbinit])
AC_CONFIG_FILES([metadata-server/metadata_server.sh],
		[chmod +x metadata-server/metadata_server.sh])

AC_CONFIG_FILES([python_scrubber/Makefile])
AC_CONFIG_FILES([python_scrubber/__init__.py])
AC_CONFIG_FILES([python_scrubber/.gdbinit])
AC_CONFIG_FILES([python_scrubber/runpython.sh], [chmod +x python_scrubber/runpython.sh])

AC_CONFIG_FILES([python_scrubber/test/Makefile])
AC_CONFIG_FILES([python_scrubber/test/scrubber_test.py])
AC_CONFIG_FILES([python_scrubber/test/pylint_test.sh], [chmod +x python_scrubber/test/pylint_test.sh])
AC_CONFIG_FILES([python_scrubber/test/python_test.sh], [chmod +x python_scrubber/test/python_test.sh])

AC_CONFIG_FILES([scrubber/Makefile])

AC_CONFIG_FILES([failovercache/Makefile])
AC_CONFIG_FILES([failovercache/.gdbinit])

AC_CONFIG_FILES([failovercache/test/failovercache_tests.sh], [chmod +x failovercache/test/failovercache_tests.sh])
AC_CONFIG_FILES([failovercache/test/failovercache_valgrind.sh], [chmod +x failovercache/test/failovercache_valgrind.sh])
AC_CONFIG_FILES([failovercache/test/Makefile])

AC_CONFIG_FILES([pythontest/Makefile])
AC_CONFIG_FILES([pythontest/python_test.sh], [chmod +x pythontest/python_test.sh])
AC_CONFIG_FILES([pythontest/pylint_test.sh], [chmod +x pythontest/pylint_test.sh])

 # Get rid of -lgcc_s automagically added to the linker flags
postdeps_CXX=`echo " $postdeps_CXX " | sed 's, -lgcc_s, -lgcc_eh,g'`

AC_OUTPUT
