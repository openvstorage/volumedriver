ACLOCAL_AMFLAGS = -I ../../m4

scrubberdir=@prefix@/lib/python@PYTHON_VERSION@/dist-packages/volumedriver/scrubber
scrubber_LTLIBRARIES = scrubber.la

noinst_DATA=runpython.sh

scrubber_la_SOURCES = \
	ScrubberModule.cpp

#	SyncstoreScrubber.cppxs
scrubber_la_LDFLAGS = -module -shared

scrubber_la_LIBADD = \
	../libvolumedriver.la \
	../../backend/libbackend.la \
	../../youtils/libyoutils.la \
	$(BUILDTOOLS_LIBS) \
	-lpthread -lrt -lm

scrubber_la_CPPFLAGS = -I@abs_top_srcdir@/..

scrubber_la_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

scrubber_PYTHON = \
	__init__.py

SUBDIRS = . test


