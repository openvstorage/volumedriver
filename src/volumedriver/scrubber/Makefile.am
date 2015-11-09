ACLOCAL_AMFLAGS=-I ../../m4

bin_PROGRAMS = \
	ovs_scrubber

ovs_scrubber_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

ovs_scrubber_LDADD = \
	../libvolumedriver.la  \
	../../backend/libbackend.la \
	../../youtils/libyoutils.la \
	$(BUILDTOOLS_LIBS)

ovs_scrubber_CPPFLAGS = \
	-I@abs_top_srcdir@/.. \
	-I@abs_top_builddir@

ovs_scrubber_LDFLAGS = -Wl,--as-needed

ovs_scrubber_SOURCES = Main.cpp

TESTS =
