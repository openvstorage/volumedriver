ACLOCAL_AMFLAGS = -I ../../m4
bin_PROGRAMS = prudence prudence_test


prudence_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
prudence_LDFLAGS = -Wl,--as-needed
prudence_LDADD = ../libyoutils.la $(BUILDTOOLS_LIBS) -lgcrypt
prudence_CPPFLAGS = \
	-I@abs_top_srcdir@/..

prudence_SOURCES = \
	prudence.cpp

prudence_test_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
prudence_test_LDFLAGS = -Wl,--as-needed
prudence_test_LDADD = ../libyoutils.la $(BUILDTOOLS_LIBS) -lgcrypt
prudence_test_CPPFLAGS = \
	-I@abs_top_srcdir@/..

prudence_test_SOURCES = \
	test/prudence_test.cpp

noinst_DATA = prudence_test.sh

TESTS = prudence_test.sh

CLEANFILE = $(noinst_SCRIPT)
