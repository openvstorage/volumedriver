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
	ArakoonLockStoreBuilder.cpp \
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
	FailOverCacheAsyncBridge.cpp \
	FailOverCacheClientInterface.cpp \
	FailOverCacheConfig.cpp \
	FailOverCacheConfigWrapper.cpp \
	FailOverCacheMode.cpp \
	FailOverCacheProxy.cpp \
	FailOverCacheStreamers.cpp \
	FailOverCacheSyncBridge.cpp \
	FailOverCacheTransport.cpp \
	FilePool.cpp \
	distributed-transaction-log/Backend.cpp \
	distributed-transaction-log/BackendFactory.cpp \
	distributed-transaction-log/Acceptor.cpp \
	distributed-transaction-log/DtlProtocol.cpp \
	distributed-transaction-log/FileBackend.cpp \
	distributed-transaction-log/MemoryBackend.cpp \
	distributed-transaction-log/fungilib/Buffer.cpp \
	distributed-transaction-log/fungilib/ByteArray.cpp \
	distributed-transaction-log/fungilib/CondVar.cpp \
	distributed-transaction-log/fungilib/Conversions.cpp \
	distributed-transaction-log/fungilib/File.cpp \
	distributed-transaction-log/fungilib/IOBaseStream.cpp \
	distributed-transaction-log/fungilib/IPv4Socket.cpp \
	distributed-transaction-log/fungilib/IPv6Socket.cpp \
	distributed-transaction-log/fungilib/Mutex.cpp \
	distributed-transaction-log/fungilib/Socket.cpp \
	distributed-transaction-log/fungilib/SocketServer.cpp \
	distributed-transaction-log/fungilib/Runnable.cpp \
	distributed-transaction-log/fungilib/Thread.cpp \
	distributed-transaction-log/fungilib/WrapByteArray.cpp \
	LocalTLogScanner.cpp \
	LockStoreFactory.cpp \
	LockStoreType.cpp \
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
	metadata-server/RocksConfig.cpp \
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
	RelocationReaderFactory.cpp \
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
	SCOWrittenToBackendAction.cpp \
	SCOPool.cpp \
	Scrubber.cpp \
	ScrubberAdapter.cpp \
	ScrubbingCleanup.cpp \
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
	distributed-transaction-log \
	metadata-server \
	pitreplication \
	python_scrubber \
	scrubber \
	ToolCut \
	pythontest
