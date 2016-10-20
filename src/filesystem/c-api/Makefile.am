ACLOCAL_AMFLAGS = -I ../../m4
LTTNG_GEN_TP = @lttng_gen_tp@

%_tp.h: %.tp
	echo "generating $@ from $<"
	$(LTTNG_GEN_TP) $< -o $@

%_tp.c: %.tp %_tp.h
	echo "generating $@ from $<"
	$(LTTNG_GEN_TP) $< -o $@

libovsvolumedriverdir = @prefix@/lib/openvstorage
libovsvolumedriver_LTLIBRARIES = libovsvolumedriver.la

libovsvolumedriver_la_SOURCES = \
	AioCompletion.cpp \
	../ShmIdlInterface.cpp \
	ShmControlChannelClient.cpp \
	ShmContext.cpp \
	ShmClient.cpp \
	ShmHandler.cpp \
	VolumeCacheHandler.cpp \
	Logger.cpp \
	NetworkHAContext.cpp \
	NetworkXioClient.cpp \
	NetworkXioContext.cpp \
	TracePoints_tp.c \
	Utils.cpp \
	libovsvolumedriver.cpp

libovsvolumedriver_la_CXXFLAGS = ${BUILDTOOLS_CFLAGS} \
	-DBOOST_DATE_TIME_POSIX_TIME_STD_CONFIG \
	-I@abs_top_srcdir@/../msgpack-c/include

libovsvolumedriver_la_LIBADD = ../../youtils/libyoutils.la \
	${BUILDTOOLS_LIBS} \
	-lxio -lgcc

libovsvolumedriver_la_CPPFLAGS = \
	-I@abs_top_srcdir@/ \
	-I@abs_top_srcdir@/.. \
	-I@abs_top_builddir@/../filesystem \
	-I@abs_top_builddir@/../volumedriver

libovsvolumedriver_nobase_includedir = @prefix@/include/openvstorage
libovsvolumedriver_nobase_include_HEADERS = volumedriver.h

SOCURRENT=0
SOREVISION=0
SOAGE=0
libovsvolumedriver_la_LDFLAGS = -shared -version-info \
	$(SOCURRENT):$(SOREVISION):$(SOAGE)

MOSTLYCLEANFILES = \
	TracePoints_tp.h \
	TracePoints_tp.c

CLEANFILES = $(bin_SCRIPTS)

BUILT_SOURCES = \
	TracePoints_tp.h \
	TracePoints_tp.c
