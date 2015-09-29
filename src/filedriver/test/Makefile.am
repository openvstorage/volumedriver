ACLOCAL_AMFLAGS = -I ../../m4

bin_PROGRAMS = filedriver_test

filedriver_test_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

filedriver_test_LDADD = \
	../libfiledriver.la  \
	../../backend/libbackend.la \
	../../youtils/libyoutils.la \
	$(BUILDTOOLS_LIBS)

filedriver_test_LDFLAGS = -Wl,--as-needed

filedriver_test_CPPFLAGS = \
	-I@abs_top_srcdir@/..

filedriver_test_SOURCES = \
	ContainerManagerTest.cpp \
	ExtentCacheTest.cpp \
	ExtentIdTest.cpp \
	Main.cpp

noinst_DATA = filedriver_tests.sh
TESTS = filedriver_tests.sh
