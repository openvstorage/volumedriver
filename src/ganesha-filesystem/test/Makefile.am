ACLOCAL_AMFLAGS = -I ../../m4

bin_PROGRAMS = volumedriverfs_ganesha_test

volumedriverfs_ganesha_test_CXXFLAGS = $(BUILDTOOLS_CFLAGS) -D_FILE_OFFSET_BITS=64
volumedriverfs_ganesha_test_CPPFLAGS = -I@abs_top_srcdir@/.. -I@abs_builddir@/..
volumedriverfs_ganesha_test_LDFLAGS = -Wl,--as-needed

volumedriverfs_ganesha_test_LDADD = \
	../../filesystem/test/libvolumedriverfstest.la \
	../../filesystem/libvolumedriverfs.la \
	../../filedriver/libfiledriver.la \
	../../volumedriver/test/libvolumedrivertest.la \
	../../volumedriver/libvolumedriver.la \
	../../xmlrpc++0.7/libxmlrpc.la \
	../../backend/libbackend.la \
	../../simple-nfs/libsimplenfs.la \
	../../youtils/libyoutils.la \
	-lnfs \
	-lfuse3 \
	$(BUILDTOOLS_LIBS)

volumedriverfs_ganesha_test_SOURCES = \
	GaneshaTestSetup.cpp \
	Main.cpp \
	SimpleTest.cpp

ganeshadir = ${datadir}/ganeshafs

noinst_DATA = volumedriverfs_ganesha_test.sh
TESTS = volumedriverfs_ganesha_test.sh
