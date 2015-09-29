ACLOCAL_AMFLAGS = -I ../../m4

toolcutdir=@prefix@/lib/python@PYTHON_VERSION@/dist-packages/volumedriver/toolcut
toolcut_LTLIBRARIES = ToolCut.la

noinst_DATA=runpython.sh

ToolCut_la_SOURCES = \
	ClusterLocationToolCut.cpp \
	EntryToolCut.cpp \
	MetadataStoreToolCut.cpp \
	SCOAccessDataInfo.cpp \
	SCOToolCut.cpp \
	ScrubbingResultToolCut.cpp \
	SnapshotPersistorToolCut.cpp \
	SnapshotToolCut.cpp \
	TLogReaderToolCut.cpp \
	TLogToolCut.cpp \
	ToolCut.cpp \
	VolumeInfo.cpp

ToolCut_la_LDFLAGS = -module -shared

ToolCut_la_LIBADD = \
	../libvolumedriver.la \
	../../backend/libbackend.la \
	../../youtils/libyoutils.la \
	$(BUILDTOOLS_LIBS) \
	-lpthread -lrt -lm

ToolCut_la_CPPFLAGS = -I@abs_top_srcdir@/..

ToolCut_la_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

toolcut_PYTHON = \
	__init__.py


SUBDIRS = . test
