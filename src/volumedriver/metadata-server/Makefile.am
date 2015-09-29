ACLOCAL_AMFLAGS = -I ../../m4
bin_PROGRAMS = metadata_server

metadata_server_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
metadata_server_CPPFLAGS = -I@abs_top_srcdir@/.. -I@abs_builddir@
metadata_server_LDADD = \
	../libvolumedriver.la \
	../../backend/libbackend.la \
	../../youtils/libyoutils.la \
	 $(BUILDTOOLS_LIBS)

metadata_server_LDFLAGS = -Wl,--as-needed
metadata_server_SOURCES = \
	Main.cpp

noinst_DATA = .gdbinit

SUBDIRS = .
