ACLOCAL_AMFLAGS = -I ../m4
PROTOC = @protoc@
LTTNG_GEN_TP = @lttng_gen_tp@

%_tp.h: %.tp
	echo "generating $@ from $<"
	$(LTTNG_GEN_TP) $< -o $@

%_tp.c: %.tp %_tp.h
	echo "generating $@ from $<"
	$(LTTNG_GEN_TP) $< -o $@

lib_LTLIBRARIES = libvolumedriverfs.la

OMNIIDL=@omniidl@

ShmIdlInterface.cpp : ShmIdlInterface.idl
	$(OMNIIDL) -bcxx -Wbh=.h -Wbs=.cpp -Wbtp $<

libvolumedriverfs_la_CXXFLAGS = \
	$(BUILDTOOLS_CFLAGS) -D_FILE_OFFSET_BITS=64 \
	-I@abs_top_srcdir@/../msgpack-c/include
libvolumedriverfs_la_CFLAGS = $(BUILDTOOLS_CFLAGS) -D_FILE_OFFSET_BITS=64
libvolumedriverfs_la_CPPFLAGS = -I@abs_top_srcdir@/.. -I@abs_top_builddir@/../volumedriver
libvolumedriverfs_la_LDFLAGS = -static

# this path trickery is necessary as protobuf imports need to start at the same root
%.pb.cc %.pb.h %_pb2.py: %.proto
	$(PROTOC) -I$(abs_top_srcdir) -I$(abs_top_srcdir)/../volumedriver --cpp_out=$(builddir) $^
	$(PROTOC) -I$(abs_top_srcdir) -I$(abs_top_srcdir)/../volumedriver --python_out=$(builddir) $^

volumedriverfsdir=@prefix@/share/volumedriverfs
volumedriverfs_DATA = \
	FileSystemEvents.proto \
	Messages.proto

nodist_libvolumedriverfs_la_SOURCES = \
	FileSystemEvents.pb.cc \
	FileSystemEvents.pb.h \
	Messages.pb.cc \
	Messages.pb.h \
	TracePoints_tp.h \
	TracePoints_tp.c

MOSTLYCLEANFILES = \
	FileSystemEvents.pb.cc \
	FileSystemEvents.pb.h \
	FileSystemEvents_pb2.py \
	Messages.pb.cc \
	Messages.pb.h \
	TracePoints_tp.h \
	TracePoints_tp.c

bin_SCRIPTS = volumedriver_fs.sh

CLEANFILES = $(bin_SCRIPTS)

# XXX: the PythonClient lives here for ease of testing
libvolumedriverfs_la_SOURCES = \
		AmqpEventPublisher.cpp \
		AmqpTypes.cpp \
		CachedObjectRegistry.cpp \
		ClusterNode.cpp \
		ClusterNodeConfig.cpp \
		ClusterNodeStatus.cpp \
		ClusterRegistry.cpp \
		ConfigFetcher.cpp \
		ConfigHelper.cpp \
		DataPoints.cpp \
		DirectoryEntry.cpp \
		EventPublisher.cpp \
		FailOverCacheConfigMode.cpp \
		FileEventRule.cpp \
		FileSystem.cpp \
		FileSystemCall.cpp \
		FileSystemEvents.cpp \
		FileSystemEvents.pb.cc \
		FileSystemParameters.cpp \
		FuseInterface.cpp \
		HierarchicalArakoon.cpp \
		LocalNode.cpp \
		LocalPythonClient.cpp \
		LockedPythonClient.cpp \
		Messages.pb.cc \
		MessageUtils.cpp \
		MetaDataStore.cpp \
		NetworkXioIOHandler.cpp \
		NetworkXioServer.cpp \
		NetworkXioInterface.cpp \
		Object.cpp \
		ObjectRegistry.cpp \
		ObjectRouter.cpp \
		ObjectTreeConfig.cpp \
		Permissions.cpp \
		Protocol.cpp \
		PythonClient.cpp \
		Registry.cpp \
		RemoteNode.cpp \
		ScrubManager.cpp \
		ScrubTreeBuilder.cpp \
		ShmIdlInterface.cpp \
		ShmInterface.cpp \
		ShmOrbInterface.cpp \
		ShmServer.cpp \
		StatsCollectorComponent.cpp \
		TracePoints_tp.c \
		VirtualDiskFormat.cpp \
		VirtualDiskFormatVmdk.cpp \
		VirtualDiskFormatRaw.cpp \
		XMLRPC.cpp \
		XMLRPCKeys.cpp \
		XMLRPCStructs.cpp \
		XMLRPCUtils.cpp \
		ZWorkerPool.cpp

volumedriverfs_pythondir = @prefix@/lib/python@PYTHON_VERSION@/dist-packages/volumedriver/storagerouter

volumedriverfs_python_PYTHON = \
	$(abs_builddir)/FileSystemEvents_pb2.py

BUILT_SOURCES = \
	FileSystemEvents.pb.h \
	FileSystemEvents_pb2.py \
	Messages.pb.h \
	TracePoints_tp.h \
	TracePoints_tp.c

bin_PROGRAMS = volumedriver_fs

volumedriver_fs_CXXFLAGS = $(BUILDTOOLS_CFLAGS) -D_FILE_OFFSET_BITS=64

volumedriver_fs_LDADD = \
	libvolumedriverfs.la \
	../filedriver/libfiledriver.la \
	../volumedriver/libvolumedriver.la \
	../xmlrpc++0.7/libxmlrpc.la \
	../backend/libbackend.la \
	../youtils/libyoutils.la \
	-lfuse3 -lxio \
	$(BUILDTOOLS_LIBS)

volumedriver_fs_CPPFLAGS = \
	-I@abs_top_srcdir@/.. \
	-I@abs_top_builddir@/../volumedriver

volumedriver_fs_LDFLAGS = -Wl,--as-needed

volumedriver_fs_SOURCES = \
	Main.cpp

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = volumedriverfs.pc

SUBDIRS = . c-api failovercache test
