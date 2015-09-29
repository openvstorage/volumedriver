ACLOCAL_AMFLAGS=-I ../../../m4
bin_PROGRAMS=failovercache_test failovercacheclient_perftest

failovercache_test_CXXFLAGS=$(BUILDTOOLS_CFLAGS)
failovercache_test_LDADD=../../libvolumedriver.la  \
	../../../backend/libbackend.la \
	../../../youtils/libyoutils.la \
	$(BUILDTOOLS_LIBS)

failovercache_test_LDFLAGS=-Wl,--as-needed
failovercache_test_CPPFLAGS=-I@abs_top_srcdir@/..
failovercache_test_SOURCES=\
	failovercache_tester.cpp \
	FailOverCacheEnvironment.cpp \
	../FailOverCacheServer.cpp \
	FailOverCacheTestMain.cpp
#	FailOverCacheTestThread.cpp


failovercacheclient_perftest_CXXFLAGS=$(BUILDTOOLS_CFLAGS)
failovercacheclient_perftest_LDADD=../../libvolumedriver.la  \
	../../../backend/libbackend.la \
	../../../youtils/libyoutils.la \
	$(BUILDTOOLS_LIBS)

failovercacheclient_perftest_LDFLAGS=-Wl,--as-needed
failovercacheclient_perftest_CPPFLAGS=-I@abs_top_srcdir@/..
failovercacheclient_perftest_SOURCES=\
	FailOverCacheClientPerfTest.cpp

noinst_DATA = failovercache_tests.sh
TESTS = failovercache_tests.sh