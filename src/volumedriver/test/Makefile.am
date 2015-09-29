ACLOCAL_AMFLAGS = -I ../../m4

lib_LTLIBRARIES = libvolumedrivertest.la

libvolumedrivertest_la_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
libvolumedrivertest_la_CPPFLAGS = \
	-I@abs_top_builddir@/ \
	-I@abs_top_srcdir@/ \
	-I@abs_top_srcdir@/..

libvolumedrivertest_la_LDFLAGS = -static

libvolumedrivertest_la_SOURCES = \
	FailOverCacheTestSetup.cpp \
	MDSTestSetup.cpp \
	MetaDataStoreTestSetup.cpp

bin_PROGRAMS = volumedriver_test

volumedriver_test_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

volumedriver_test_LDADD = \
	./libvolumedrivertest.la \
	../libvolumedriver.la  \
	../../backend/libbackend.la \
	../../fawltyfs/libfawltyfs.la \
	../../youtils/libyoutils.la \
	$(BUILDTOOLS_LIBS)

volumedriver_test_LDFLAGS = -Wl,--as-needed

volumedriver_test_CPPFLAGS = \
	-I@abs_top_srcdir@/.. \
	-I@abs_top_builddir@/ \
	-I@abs_top_builddir@/../fawltyfs/corba-daemon

volumedriver_test_SOURCES = \
	ApiTest.cpp \
	BackwardsCompatibilityTest.cpp \
	BigReadWriteTest.cpp \
	CachedSCOTest.cpp \
	cases.cpp \
	CloneManagementTest.cpp \
	CloneVolumeTest.cpp \
	ClusterCacheSerializationTest.cpp \
	ClusterCacheMapTest.cpp \
	ClusterCacheTest.cpp \
	ClusterLocationTest.cpp \
	DataStoreNGTest.cpp \
	DestroyVolumeTest.cpp \
	EntryTest.cpp \
	ErrorHandlingTest.cpp \
	EventCollector.cpp \
	ExGTest.cpp \
	FailOverCacheTester.cpp \
	FailOverCacheTransportTest.cpp \
	FawltyTests.cpp \
	FawltyCorba.cpp \
	FileDescriptorResourceLimitTest.cpp \
	KakPerformanceTest.cpp \
	Literals.cpp \
	LocalRestartTest.cpp \
	LocalRestartTestNoBackend.cpp \
	LockingTest.cpp \
	MDSManagerTest.cpp \
	MDSMetaDataStoreTest.cpp \
	MDSVolumeTest.cpp \
	MetaDataBackendConfigTest.cpp \
	MetaDataServerTest.cpp \
	MetaDataServerProtocolTest.cpp \
	MetaDataStoreTest.cpp \
	MetaDataStoreBuilderTest.cpp \
	MTVolumeTester.cpp \
	OwnerTagTest.cpp \
	PageSortingGeneratorTest.cpp \
	Panacea.cpp \
	PrefetchThreadTest.cpp \
	ProducerConsumerTest.cpp \
	ReadParallelismTest.cpp \
	ResourceLimitTest.cpp \
	RocksTest.cpp \
	SCOAccessDataTest.cpp \
	SCOCacheConstructorTest.cpp \
	SCOCacheMapTest.cpp \
	SCOCacheNamespaceTest.cpp \
	SCOCacheTest.cpp \
	ScrubberTest.cpp \
	ScrubWorkTest.cpp \
	SimpleBackupRestoreTest.cpp \
	SimpleVolumeTest.cpp \
	SnapshotManagementTest.cpp \
	SnapshotPersistorTest.cpp \
	SnapshotRestoreTest.cpp \
	StatusWriterTest.cpp \
	StreamEnumTest.cpp \
	TCBTMetaDataStoreSimTest.cpp \
	TemplateVolumeTest.cpp \
	Test2TCMaps.cpp \
	TestMetadataErrors.cpp \
	ThrottlingTest.cpp \
	TimeZoneTests.cpp \
	TLogTest.cpp \
	TokyoCabinetMetaDataStoreTest.cpp \
	ToolCutTest.cpp \
	vd_test.cpp \
	VolManagerRestartTest.cpp \
	VolManagerTestSetup.cpp \
	VolManagerThreadTest.cpp \
	VolManagerTLogSCOWrapTest.cpp \
	VolManagerVolumeDestroy.cpp \
	VolumeBackupTest.cpp \
	VolumeConfigParametersTest.cpp \
	VolumeDriverConfigurationTest.cpp \
	VolumeDriverErrorTest.cpp \
	VolumeDriverInitTest.cpp \
	VolumeStateManagementTest.cpp \
	VolumeTest.cpp \
	WriteOnlyVolumeTest.cpp \
	XMLRPCShutDownTest.cpp

#	FeatureTest.cpp
#	FreezeAndCloneFreezeVolumeTest.cpp
# 	GrandClusterCacheTest.cpp
#       LockedFunctionTest.cpp
#	SnapshotsTLogsTest.cpp
#	TestMoveSemanticsAndMap.cpp
#	VolumeBackupTestSetup.cpp
#	VolumeBackupErrorHandlingTest.cpp

noinst_DATA = volumedriver_tests.sh
TESTS = volumedriver_tests.sh
