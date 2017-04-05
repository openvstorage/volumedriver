ACLOCAL_AMFLAGS=-I ../../m4
bin_PROGRAMS=fawltyfs_test

fawltyfs_test_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
fawltyfs_test_CPPFLAGS = -I@abs_top_srcdir@/.. -D_FILE_OFFSET_BITS=64
fawltyfs_test_LDADD = \
	../libfawltyfs.la \
	../../youtils/libyoutils.la \
	-lfuse -lgcrypt \
	$(BUILDTOOLS_LIBS)

fawltyfs_test_LDFLAGS=-Wl,--as-needed
fawltyfs_test_SOURCES= \
	FileSystemTest.cpp \
	FileSystemTestSetup.cpp \
	FuseFSTest.cpp \
	FuseFSTestSetup.cpp \
	Main.cpp

noinst_DATA = fawltyfs_test.sh
TESTS = fawltyfs_test.sh
