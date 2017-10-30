ACLOCAL_AMFLAGS = -I ../m4

noinst_LTLIBRARIES = \
	libchecksum.la

# We only want to enable -msse4.2 for the CheckSum code as there we check on load
# time if SSE4.2 is available and fall back to a S/W solution otherwise. This does
# not hold for other potential users, e.g. uuid code, ...
libchecksum_la_CXXFLAGS = -msse4.2 $(BUILDTOOLS_CFLAGS)
libchecksum_la_CPPFLAGS = -I@abs_top_srcdir@/..
libchecksum_la_LDFLAGS = -static

libchecksum_la_SOURCES = \
	CheckSum.cpp

lib_LTLIBRARIES = \
	libyoutils.la

if CLANGPP
PCH_SUFFIX = pch
else
PCH_SUFFIX = gch
endif

BUILT_SOURCES = LibyoutilsPrecompiledHeader.h.$(PCH_SUFFIX)

LibyoutilsPrecompiledHeader.h.$(PCH_SUFFIX) : LibyoutilsPrecompiledHeader.h.hxx
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) $(libyoutils_la_CXXFLAGS) $(libyoutils_la_CPPFLAGS) $(CXXFLAGS) -MT $@ -MD -MP -MF $(DEPDIR)/LibyoutilsPrecompiledHeader.Tpo -x c++-header -o $@ $<
	$(am__mv) $(DEPDIR)/LibyoutilsPrecompiledHeader.Tpo $(DEPDIR)/LibyoutilsPrecompiledHeader.Po

-include ./$(DEPDIR)/LibyoutilsPrecompiledHeader.Po

libyoutils_la_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

# if you want to disable precompiled headers or drop
# `-include LibyoutilsPrecompiledHeader.h'
libyoutils_la_CPPFLAGS = \
	-I@abs_top_builddir@ \
	-I@abs_top_srcdir@ \
	-I@abs_top_srcdir@/.. \
	-Winvalid-pch
#	-include LibyoutilsPrecompiledHeader.h

libyoutils_la_LDFLAGS = -static

libyoutils_la_SOURCES = \
	ArakoonConfigFetcher.cpp \
	ArakoonIniParser.cpp \
	ArakoonInterface.cpp \
	ArakoonLockStore.cpp \
	ArakoonNodeConfig.cpp \
	ArakoonTestSetup.cpp \
	BuildInfo.cpp \
	BuildInfoString.cpp \
	Catchers.cpp \
	Chooser.cpp \
	ConfigurationReport.cpp \
	ConfigFetcher.cpp \
	CorbaTestSetup.cpp \
	cpu_timer.cpp \
	DeferredFileRemover.cpp \
	DimensionedValue.cpp \
	EtcdConfigFetcher.cpp \
	EtcdReply.cpp \
	FileDescriptor.cpp \
	FileRange.cpp \
	FileUtils.cpp \
	Gcrypt.cpp \
	Generator.cpp \
	GlobalLockService.cpp \
	GlobalLockedCallable.cpp \
	IncrementalChecksum.cpp \
	InitializedParam.cpp \
	IOException.cpp \
	HeartBeat.cpp \
	HeartBeatLock.cpp \
	HeartBeatLockCommunicator.cpp \
	HeartBeatLockService.cpp \
	LockedArakoon.cpp \
	LocORemClient.cpp \
	LocORemConnection.cpp \
	LocORemServer.cpp \
	Logger.cpp \
	LoggerToolCut.cpp \
	LoggingToolCut.cpp \
	LRUCache.cpp \
	MainEvent.cpp \
	MainHelper.cpp \
	Md5.cpp \
	Notifier.cpp \
	OptionValidators.cpp \
	OrbHelper.cpp \
	OrbParameters.cpp \
	PeriodicAction.cpp \
	PeriodicActionPool.cpp \
	python/ArakoonClient.cpp \
	PythonLogging.cpp \
	PythonBuildInfo.cpp \
	RedisQueue.cpp \
	RocksLogger.cpp \
	RWLock.cpp \
	ScriptFile.cpp \
	SerializableDynamicBitset.cpp \
	Sha1.cpp \
	SharedMemoryRegion.cpp \
	SharedMemoryRegionId.cpp \
	SignalBlocker.cpp \
	SignalSet.cpp \
	SignalThread.cpp \
	SocketAddress.cpp \
	SpinLock.cpp \
	StreamUtils.cpp \
	StrongTypedString.cpp \
	System.cpp \
	TestMainHelper.cpp \
	ThreadPool.cpp \
	ThrowingDestructor.cpp \
	Time.cpp \
	Tracer.cpp \
	UniqueObjectTag.cpp \
	UpdateReport.cpp \
	Uri.cpp \
	UUID.cpp \
	VolumeDriverComponent.cpp \
	WaitForIt.cpp \
	wall_timer.cpp \
	WithGlobalLock.cpp

libyoutils_la_LIBADD = \
	libchecksum.la

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = youtils.pc

SUBDIRS = . prudence test
# corbatest

@abs_top_srcdir@/BuildInfo.cpp : buildinfo.sh
	./buildinfo.sh
