ACLOCAL_AMFLAGS = -I ../m4

LTTNG_GEN_TP = @lttng_gen_tp@

%_tp.h: %.tp
	echo "generating $@ from $<"
	$(LTTNG_GEN_TP) $< -o $@

%_tp.c: %.tp %_tp.h
	echo "generating $@ from $<"
	$(LTTNG_GEN_TP) $< -o $@

libfsalovsdir=@prefix@/lib/ganesha
libfsalovs_LTLIBRARIES = libfsalovs.la

openvstorageconfigdir=@prefix@/opt/OpenvStorage/config/templates

libfsalovs_la_SOURCES = \
	Tracepoints_tp.h \
	Tracepoints_tp.c \
	FileSystemWrapper.cpp \
	Common.cpp \
	Handle.cpp \
	Export.cpp \
	TCPServer.cpp \
	Main.cpp

libfsalovs_la_LDFLAGS = -module -shared

libfsalovs_la_LIBADD = \
	../filesystem/libvolumedriverfs.la \
	../filedriver/libfiledriver.la \
	../volumedriver/libvolumedriver.la \
	../xmlrpc++0.7/libxmlrpc.la \
	../backend/libbackend.la \
	../youtils/libyoutils.la \
	-lfuse \
	$(BUILDTOOLS_LIBS) \
	-lpthread -lrt -lm

bin_SCRIPTS = ganesha-cf

openvstorageconfig_DATA = ganesha-cf.conf

libfsalovs_la_CXXFLAGS = \
	$(BUILDTOOLS_CFLAGS) \
	-I@abs_top_srcdir@/.. \
	-I@abs_top_builddir@/../volumedriver \
	-I@abs_top_builddir@/../filesystem \
	-I@abs_top_srcdir@/../msgpack-c/include \
	-I@ganesha@/ganesha \
	-I@ganesha@/ntirpc

libfsalovs_la_CFLAGS = $(BUILDTOOLS_CFLAGS)


install-exec-local:
	cp @buildtoolsdir@/bin/ganesha.nfsd ${prefix}/bin

SUBDIRS = . test
