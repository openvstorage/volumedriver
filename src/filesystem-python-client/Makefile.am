# find a better name + move the C++ source files to a distinct directory
storagerouterclientdir = \
	@prefix@/lib/python@PYTHON_VERSION@/dist-packages/volumedriver/storagerouter

storagerouterclient_LTLIBRARIES = storagerouterclient.la

noinst_DATA = runpython.sh

storagerouterclient_la_SOURCES = \
	ArakoonClient.cpp \
	FileSystemMetaDataClient.cpp \
	LocalClient.cpp \
	LockedClient.cpp \
	MDSClient.cpp \
	ObjectRegistryClient.cpp \
	ScrubManagerClient.cpp \
	StorageRouterClient.cpp \
	PythonTestHelpers.cpp

storagerouterclient_la_LDFLAGS = -module -shared

storagerouterclient_la_LIBADD = \
	../xmlrpc++0.7/libxmlrpc.la \
	../filesystem/libvolumedriverfs.la \
	../filedriver/libfiledriver.la \
	../volumedriver/libvolumedriver.la \
	../backend/libbackend.la \
	../youtils/libyoutils.la \
	-lfuse3 \
	$(BUILDTOOLS_LIBS) \
	-lpthread -lrt -lm -lgcc

storagerouterclient_la_CPPFLAGS = \
	-I@abs_top_srcdir@/.. \
	-I@abs_top_builddir@/../volumedriver

storagerouterclient_la_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

storagerouterclient_PYTHON = \
	__init__.py

SUBDIRS = . test
