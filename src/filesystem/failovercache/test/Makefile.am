ACLOCAL_AMFLAGS = -I ../../../m4
bin_PROGRAMS = failovercachehelpertest

failovercachehelpertest_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
failovercachehelpertest_CPPFLAGS = -I@abs_top_srcdir@/..
failovercachehelpertest_LDADD = \
	../../libvolumedriverfs.la \
	../../../volumedriver/libvolumedriver.la \
	../../../xmlrpc++0.7/libxmlrpc.la \
	../../../backend/libbackend.la \
	../../../youtils/libyoutils.la \
	 $(BUILDTOOLS_LIBS)

failovercachehelpertest_LDFLAGS = -Wl,--as-needed
failovercachehelpertest_SOURCES = \
	failovercachehelpertest.cpp


