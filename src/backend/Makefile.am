CLOCAL_AMFLAGS = -I ../m4
BUILDTOOLS_DIR=@buildtoolsdir@

bin_SCRIPTS = delete-test-namespace.sh alba-start-script.sh alba-stop-script.sh arakoon_cfg_template.ini

lib_LTLIBRARIES = libbackend.la

libbackend_la_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
libbackend_la_CFLAGS = $(BUILDTOOLS_CFLAGS)
libbackend_la_CPPFLAGS = -I@abs_top_srcdir@/..
libbackend_la_LDFLAGS = -static

LTTNG_GEN_TP = @lttng_gen_tp@

%_tp.h: %.tp
	echo "generating $@ from $<"
	$(LTTNG_GEN_TP) $< -o $@

%_tp.c: %.tp %_tp.h
	echo "generating $@ from $<"
	$(LTTNG_GEN_TP) $< -o $@

MOSTLYCLEANFILES = \
	BackendTracePoints_tp.c \
	BackendTracePoints_tp.h

BUILT_SOURCES = \
	BackendTracePoints_tp.c \
	BackendTracePoints_tp.h

#CLEANFILES=${BUILT_SOURCES}

libbackend_la_SOURCES = \
	AlbaConfig.cpp \
	Alba_Connection.cpp \
	BackendConfig.cpp \
	BackendConnectionManager.cpp \
	BackendConnectionInterface.cpp \
	BackendInterface.cpp \
	BackendTracePoints_tp.c \
	BackendParameters.cpp \
	BackendTestSetup.cpp \
	GarbageCollector.cpp \
	LocalConfig.cpp \
	Local_Connection.cpp \
	Local_Sink.cpp \
	Local_Source.cpp \
	LockStore.cpp \
	ManagedBackendSink.cpp \
	ManagedBackendSource.cpp \
	MultiConfig.cpp \
	Multi_Connection.cpp \
	Namespace.cpp \
	S3Config.cpp \
	S3_Connection.cpp \
	SimpleFetcher.cpp \
	TestWithBackendMainHelper.cpp

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = backend.pc

SUBDIRS = . test
