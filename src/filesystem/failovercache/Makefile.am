ACLOCAL_AMFLAGS = -I ../../m4
bin_PROGRAMS = failovercachehelper

failovercachehelper_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
failovercachehelper_CPPFLAGS = -I@abs_top_srcdir@/..
failovercachehelper_LDADD = \
	../libvolumedriverfs.la \
	../../volumedriver/libvolumedriver.la \
	../../xmlrpc++0.7/libxmlrpc.la \
	../../backend/libbackend.la \
	../../youtils/libyoutils.la \
	 $(BUILDTOOLS_LIBS)

failovercachehelper_LDFLAGS = -Wl,--as-needed
failovercachehelper_SOURCES = \
	FailOverCacheHelperMain.cpp

SUBDIRS = . test
