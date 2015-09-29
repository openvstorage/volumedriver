ACLOCAL_AMFLAGS = -I ../../m4
bin_PROGRAMS = failovercache

failovercache_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
failovercache_CPPFLAGS = -I@abs_top_srcdir@/..
failovercache_LDADD = \
	../libvolumedriver.la \
	../../backend/libbackend.la \
	../../youtils/libyoutils.la \
	 $(BUILDTOOLS_LIBS)

failovercache_LDFLAGS = -Wl,--as-needed
failovercache_SOURCES = \
	server.cpp \
	FailOverCacheServer.cpp

noinst_DATA = .gdbinit

SUBDIRS = . test