ACLOCAL_AMFLAGS=-I ../../m4
bin_PROGRAMS=backend_test

backend_test_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
backend_test_CPPFLAGS = -I@abs_top_srcdir@/..
backend_test_LDADD = \
	../libbackend.la \
	../../youtils/libyoutils.la \
	$(BUILDTOOLS_LIBS)

backend_test_LDFLAGS = -Wl,--as-needed
backend_test_SOURCES = \
	AlbaBackendTest.cpp \
	BackendInterfaceTest.cpp \
	BackendLoggingTest.cpp \
	BackendNamespaceTest.cpp \
	BackendObjectTest.cpp \
	BackendParametersTest.cpp \
	BackendPerfTest.cpp \
	BackendStreamTest.cpp \
	BackendTestBase.cpp \
	ConnectionManagerTest.cpp \
	GarbageCollectorTest.cpp \
	MultiBackendTest.cpp \
	NamespaceTest.cpp \
	PartialReadTest.cpp \
	S3BackendTest.cpp \
	SerializationTest.cpp \
	main.cpp

EXTRA_backend_test_DEPENDENCIES = Makefile

noinst_DATA = backend_test.sh
TESTS = backend_test.sh
