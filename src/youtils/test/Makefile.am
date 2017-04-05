ACLOCAL_AMFLAGS = -I ../../m4
bin_PROGRAMS = youtils_test mainhelpertest_executable testmainhelpertest_executable

# if you want to disable precompiled headers drop
# `-include LibyoutilsPrecompiledHeader.h'
youtils_test_CPPFLAGS = \
	-I@abs_top_srcdir@/.. \
	-I@abs_top_builddir@/test \
	-Winvalid-pch
#	-include YoutilsTestPrecompiledHeader.h

youtils_test_CXXFLAGS = $(BUILDTOOLS_CFLAGS) -Winvalid-pch
youtils_test_LDFLAGS = -Wl,--as-needed
youtils_test_LDADD = ../libyoutils.la $(BUILDTOOLS_LIBS) -lgcrypt

if CLANGPP
PCH_SUFFIX = pch
else
PCH_SUFFIX = gch
endif

BUILT_SOURCES = YoutilsTestPrecompiledHeader.h.$(PCH_SUFFIX)

YoutilsTestPrecompiledHeader.h.$(PCH_SUFFIX) : YoutilsTestPrecompiledHeader.h.hxx
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) $(youtils_test_CPPFLAGS) $(youtils_test_CXXFLAGS) $(CXXFLAGS) -MT $@ -MD -MP -MF $(DEPDIR)/YoutilsTestPrecompiledHeader.Tpo -x c++-header -o $@ $<
	$(am__mv) $(DEPDIR)/YoutilsTestPrecompiledHeader.Tpo $(DEPDIR)/YoutilsTestPrecompiledHeader.Po

youtils_test_SOURCES = \
	AlternativeOptionsAgainTest.cpp \
	ArakoonConfigFetcherTest.cpp \
	ArakoonTest.cpp \
	ArakoonIniParserTest.cpp \
	ArakoonLockStoreTest.cpp \
	BooleanEnumTest.cpp \
	CheckSumTest.cpp \
	ChooserTest.cpp \
	ChronoUtilsTest.cpp \
	ConfigFetcherTest.cpp \
	DeferredFileRemoverTest.cpp \
	DenyLockService.cpp \
	DimensionedValueTest.cpp \
	EtcdReplyTest.cpp \
	EtcdUrlTest.cpp \
	FileRangeTest.cpp \
	FileDescriptorTest.cpp \
	FileUtilsTest.cpp \
	GcryptTest.cpp \
	GeneratorTest.cpp \
	GlobalLockTest.cpp \
	Go.cpp \
	GTestTest.cpp \
	HeartBeatLockTest.cpp \
	IncrementalChecksumTest.cpp \
	InitializedParamTest.cpp \
	LocORemTest.cpp \
	LoggingTest.cpp \
	LRUCacheTest.cpp \
	LRUCacheTooTest.cpp \
	main.cpp \
	MainHelperTest.cpp \
	MTThreadPoolTest.cpp \
	OurStrongTypedefTest.cpp \
	PathTest.cpp \
	PeriodicActionTest.cpp \
	PeriodicActionPoolTest.cpp \
	PosixMQTest.cpp \
	RedisQueue.cpp \
	RWLockTest.cpp \
	ScopedExitTest.cpp \
	SerializableDynamicBitsetTest.cpp \
	SerializationTest.cpp \
	SignalHandlingTest.cpp \
	SpinLockTest.cpp \
	StrongTypedPathTest.cpp \
	StrongTypedStringTest.cpp \
	SystemTest.cpp \
	TestGlobalLockService.cpp \
	TestWithDir.cpp \
	ThreadPoolErrorHandlingTest.cpp \
	ThreadPoolPerfTest.cpp \
	ThreadPoolTest.cpp \
	ThrowingDestructorTest.cpp \
	TimeTest.cpp \
	UnlockingGlobalLockService.cpp \
	UriTest.cpp \
	UUIDTest.cpp \
	VolumeDriverComponentTest.cpp \
	WeedTest.cpp \
	WrapperTest.cpp


-include ./$(DEPDIR)/YoutilsTestPrecompiledHeader.Po


precompiled_headers : YoutilsTestPrecompiledHeader.h.pch MainhelpertestExecutablePrecompiledHeader.h.pch TestmainhelperTestExecutablePrecompiledHeader.h.pch

mainhelpertest_executable_CPPFLAGS = -I@abs_top_builddir@/test
mainhelpertest_executable_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
mainhelpertest_executable_LDFLAGS = -Wl,--as-needed
mainhelpertest_executable_LDADD = ../libyoutils.la  $(BUILDTOOLS_LIBS) -lgcrypt
mainhelpertest_executable_SOURCES = MainHelperTestExecutable.cpp


testmainhelpertest_executable_CPPFLAGS = -I@abs_top_builddir@/test
testmainhelpertest_executable_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
testmainhelpertest_executable_LDFLAGS = -Wl,--as-needed
testmainhelpertest_executable_LDADD = ../libyoutils.la  $(BUILDTOOLS_LIBS) -lgcrypt
testmainhelpertest_executable_SOURCES = TestMainHelperTestExecutable.cpp


MainhelpertestExecutablePrecompiledHeader.h.pch : MainhelpertestExecutablePrecompiledHeader.h
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	$(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) -o $@ $<

TestmainhelperTestExecutablePrecompiledHeader.h.pch : TestmainhelperTestExecutablePrecompiledHeader.h
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	$(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) -o $@ $<


CLEANFILES=YoutilsTestPrecompiledHeader.cpp YoutilsTestPrecompiledHeader.h.pch $(DEPDIR)/YoutilsTestPrecompiledHeader.Tpo \
MainhelpertestExecutablePrecompiledHeader.h.pch TestmainhelperTestExecutablePrecompiledHeader.h.pch

noinst_DATA = youtils_test.sh
TESTS = youtils_test.sh
#TESTS = youtils_test.sh logging_speed_test.sh
