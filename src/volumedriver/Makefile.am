ACLOCAL_AMFLAGS = -I ../m4
PROTOC = @protoc@

lib_LTLIBRARIES = libvolumedriver.la

libvolumedriver_la_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
libvolumedriver_la_CFLAGS = $(BUILDTOOLS_CFLAGS)
libvolumedriver_la_CPPFLAGS = -I@abs_top_srcdir@/..
libvolumedriver_la_LDFLAGS = -static

BUILDTOOLS_DIR = @buildtoolsdir@
CAPNPC = ${BUILDTOOLS_DIR}/bin/capnpc
PATH := ${BUILDTOOLS_DIR}/bin:${PATH}
LTTNG_GEN_TP = @lttng_gen_tp@

%_tp.h: %.tp
	echo "generating $@ from $<"
	$(LTTNG_GEN_TP) $< -o $@

%_tp.c: %.tp %_tp.h
	echo "generating $@ from $<"
	$(LTTNG_GEN_TP) $< -o $@

%.pb.cc %.pb.h %_pb2.py: %.proto
	$(PROTOC) -I$(abs_top_srcdir) --cpp_out=$(builddir) $^
	$(PROTOC) -I$(abs_top_srcdir) --python_out=$(builddir) $^

libvolumedriver_pythondir = @prefix@/lib/python@PYTHON_VERSION@/dist-packages/volumedriver/storagerouter

libvolumedriver_python_PYTHON = \
	$(abs_builddir)/Events_pb2.py \
	$(abs_builddir)/VolumeDriverEvents_pb2.py

libvolumedriverdir=@prefix@/share/volumedriver
libvolumedriver_DATA = \
	Events.proto \
	VolumeDriverEvents.proto

MOSTLYCLEANFILES = \
	Events.pb.cc \
	Events.pb.h \
	Events_pb2.py \
	TracePoints_tp.h \
	TracePoints_tp.c \
	VolumeDriverEvents.pb.cc \
	VolumeDriverEvents.pb.h \
	VolumeDriverEvents_pb2.py \
	metadata-server/Protocol.capnp.h \
	metadata-server/Protocol-capnp.cpp

BUILT_SOURCES = \
	Events.pb.h \
	Events_pb2.py \
	TracePoints_tp.h \
	TracePoints_tp.c \
	VolumeDriverEvents.pb.h \
	VolumeDriverEvents_pb2.py \
	metadata-server/Protocol-capnp.cpp

metadata-server/Protocol-capnp.cpp: metadata-server/Protocol.capnp
	-mkdir -p @abs_builddir@/metadata-server
	$(CAPNPC) -I${BUILDTOOLS_DIR}/include -oc++:@abs_builddir@/metadata-server/ @abs_srcdir@/metadata-server/Protocol.capnp
	mv @abs_srcdir@/metadata-server/Protocol.capnp.c++ @abs_builddir@/metadata-server/Protocol-capnp.cpp
	mv @abs_srcdir@/metadata-server/Protocol.capnp.h @abs_builddir@/metadata-server/

bin_SCRIPTS = metadata-server/metadata_server.sh

CLEANFILES = $(bin_SCRIPTS)

libvolumedriver_la_SOURCES = \
	Api.cpp \
	ArakoonMetaDataBackend.cpp \
	BackendNamesFilter.cpp \
	BackendRestartAccumulator.cpp \
	Backup.cpp \
	BackwardTLogReader.cpp \
	CachedSCO.cpp \
	CachedMetaDataPage.cpp \
	CachedMetaDataStore.cpp \
	ClusterCache.cpp \
	ClusterCacheBehaviour.cpp \
	ClusterCacheDevice.cpp \
	ClusterCacheDeviceT.cpp \
	ClusterCacheDiskStore.cpp \
	ClusterCacheDeviceManagerT.cpp \
	ClusterCacheMap.cpp \
	ClusterCacheMode.cpp \
	ClusterLocationAndHash.cpp \
	ClusterLocation.cpp \
	DataStoreNG.cpp \
	DebugPrint.cpp \
	DeleteSnapshot.cpp \
	BackendTasks.cpp \
	Entry.cpp \
	Events.pb.cc \
	FailOverCacheBridge.cpp \
	FailOverCacheConfig.cpp \
	FailOverCacheConfigWrapper.cpp \
	FailOverCacheProxy.cpp \
	FailOverCacheStreamers.cpp \
	FailOverCacheTransport.cpp \
	FilePool.cpp \
	failovercache/FailOverCacheAcceptor.cpp \
	failovercache/FailOverCacheProtocol.cpp \
	failovercache/FailOverCacheWriter.cpp \
	failovercache/VolumeFailOverCacheWriterMap.cpp \
	failovercache/fungilib/Buffer.cpp \
	failovercache/fungilib/ByteArray.cpp \
	failovercache/fungilib/CondVar.cpp \
	failovercache/fungilib/Conversions.cpp \
	failovercache/fungilib/File.cpp \
	failovercache/fungilib/IOBaseStream.cpp \
	failovercache/fungilib/IPv4Socket.cpp \
	failovercache/fungilib/IPv6Socket.cpp \
	failovercache/fungilib/Mutex.cpp \
	failovercache/fungilib/Socket.cpp \
	failovercache/fungilib/SocketServer.cpp \
	failovercache/fungilib/Runnable.cpp \
	failovercache/fungilib/Thread.cpp \
	failovercache/fungilib/WrapByteArray.cpp \
	LocalTLogScanner.cpp \
	MDSMetaDataBackend.cpp \
	MDSMetaDataStore.cpp \
	MDSNodeConfig.cpp \
	MetaDataBackendConfig.cpp \
	MetaDataStoreBuilder.cpp \
	MetaDataStoreInterface.cpp \
	MetaDataStoreDebug.cpp \
	MetaDataBackendInterface.cpp \
	metadata-server/ClientNG.cpp \
	metadata-server/DataBase.cpp \
	metadata-server/Interface.cpp \
	metadata-server/Manager.cpp \
	metadata-server/Parameters.cpp \
	metadata-server/Protocol.cpp \
	metadata-server/Protocol-capnp.cpp \
	metadata-server/PythonClient.cpp \
	metadata-server/RocksDataBase.cpp \
	metadata-server/RocksTable.cpp \
	metadata-server/ServerConfig.cpp \
	metadata-server/ServerNG.cpp \
	metadata-server/Table.cpp \
	metadata-server/Utils.cpp \
	MountPointConfig.cpp \
	NormalizedSCOAccessData.cpp \
	NSIDMap.cpp \
	OneFileTLogReader.cpp \
	OpenSCO.cpp \
	PartScrubber.cpp \
	PerformanceCounters.cpp \
	PrefetchData.cpp \
	PythonScrubber.cpp \
	RocksDBMetaDataBackend.cpp \
	RestartContext.cpp \
	Restore.cpp \
	SCOAccessData.cpp \
	SCOCache.cpp \
	SCOCacheAccessDataPersistor.cpp \
	SCOCacheMountPoint.cpp \
	SCOCacheNamespace.cpp \
	SCO.cpp \
	SCOFetcher.cpp \
	SCOPool.cpp \
	Scrubber.cpp \
	ScrubberAdapter.cpp \
	ScrubWork.cpp \
	ScrubReply.cpp \
	ScrubbingSCOData.cpp \
	SetupHelper.cpp \
	Snapshot.cpp \
	SnapshotManagement.cpp \
	SnapshotPersistor.cpp \
	StatusWriter.cpp \
	TheSonOfTLogCutter.cpp \
	TLog.cpp \
	TLogId.cpp \
	TLogCutter.cpp \
	TLogMerger.cpp \
	TLogReader.cpp \
	TLogReaderInterface.cpp \
	TLogReaderUtils.cpp \
	TLogSplitter.cpp \
	TLogWriter.cpp \
	TokyoCabinetMetaDataBackend.cpp \
	TracePoints_tp.c \
	TransientException.cpp \
	VolManager.cpp \
	VolumeConfig.cpp \
	VolumeConfigParameters.cpp \
	Volume.cpp \
	VolumeDriverParameters.cpp \
	VolumeDriverError.cpp \
	VolumeDriverEvents.pb.cc \
	VolumeFactory.cpp \
	VolumeFailOverState.cpp \
	VolumeOverview.cpp \
	VolumeThreadPool.cpp \
	WriteOnlyVolume.cpp \
	WriteSCOCache.cpp

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = volumedriver.pc

SUBDIRS = \
	. \
	test \
	ToolCut \
	python_scrubber \
	pitreplication \
	failovercache \
	metadata-server \
	pythontest
