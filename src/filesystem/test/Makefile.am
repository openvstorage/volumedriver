ACLOCAL_AMFLAGS = -I ../../m4
PROTOC = @protoc@

lib_LTLIBRARIES = libvolumedriverfstest.la

libvolumedriverfstest_la_CXXFLAGS = $(BUILDTOOLS_CFLAGS) -D_FILE_OFFSET_BITS=64

libvolumedriverfstest_la_CPPFLAGS = \
	-I@abs_top_srcdir@/.. \
	-I@abs_top_builddir@ \
	-I@abs_top_builddir@/../volumedriver

libvolumedriverfstest_la_LDFLAGS = -static

libvolumedriverfstest_la_SOURCES = \
	FileSystemTestSetup.cpp

bin_PROGRAMS = volumedriver_fs_test

volumedriver_fs_test_CXXFLAGS = $(BUILDTOOLS_CFLAGS) -D_FILE_OFFSET_BITS=64
volumedriver_fs_test_CPPFLAGS = \
	-I@abs_top_srcdir@/.. \
	-I/@abs_top_builddir@ \
	-I@abs_top_builddir@/../volumedriver

volumedriver_fs_test_LDFLAGS = -Wl,--as-needed

volumedriverfs_pytestdir = @prefix@/lib/python@PYTHON_VERSION@/dist-packages/volumedrivertest/filesystem

%.pb.cc %.pb.h: %.proto
	$(PROTOC) --proto_path=$(srcdir) --cpp_out=$(builddir) $^

volumedriverfs_pytest_PYTHON = \
	event_message_test.py

volumedriver_fs_test_LDADD = \
	./libvolumedriverfstest.la \
	../../filesystem/libvolumedriverfs.la \
	../../filesystem/c-api/libovsvolumedriver.la \
	../../filedriver/libfiledriver.la \
	../../volumedriver/test/libvolumedrivertest.la \
	../../volumedriver/libvolumedriver.la \
	../../xmlrpc++0.7/libxmlrpc.la \
	../../backend/libbackend.la \
	../../youtils/libyoutils.la \
	-lfuse3 \
	$(BUILDTOOLS_LIBS) \
	-lpython2.7 \
	-lomniORB4 \
	-lomnithread \
	-lxio

volumedriver_fs_test_SOURCES = \
	CachedObjectRegistryTest.cpp \
	ClusterRegistryTest.cpp \
	ConfigurationTest.cpp \
	DirectoryTest.cpp \
	EventPublisherTest.cpp \
	FailOverCacheHelperTest.cpp \
	FileEventRuleTest.cpp \
	FileSystemCallTest.cpp \
	FileSystemEventTest.cpp \
	FileSystemEventTestSetup.cpp \
	FileSystemTestBase.cpp \
	FileTest.cpp \
	HierarchicalArakoonTest.cpp \
	InodeAllocatorTest.cpp \
	LocalNodeTest.cpp \
	LocalPythonClientTest.cpp \
	Main.cpp \
	MessageTest.cpp \
	MetaDataStoreTest.cpp \
	NetworkServerTest.cpp \
	ObjectRegistryTest.cpp \
	ObjectRouterTest.cpp \
	ProtobufTest.cpp \
	PythonClientTest.cpp \
	RegistryTest.cpp \
	RegistryTestSetup.cpp \
	RemoteTest.cpp \
	RestartTest.cpp \
	ScrubManagerTest.cpp \
	ScrubTreeBuilderTest.cpp \
	ShmServerTest.cpp \
	StatsCollectorTest.cpp \
	VolumeTest.cpp \
	ZMQTest.cpp \
	ZWorkerPoolTest.cpp

nodist_volumedriver_fs_test_SOURCES = \
	ProtobufTest.pb.cc \
	ProtobufTest.pb.h

MOSTLYCLEANFILES = \
	ProtobufTest.pb.cc \
	ProtobufTest.pb.h

BUILT_SOURCES = \
	ProtobufTest.pb.h

noinst_DATA = volumedriver_fs_test.sh python_test.sh
TESTS = volumedriver_fs_test.sh python_test.sh
