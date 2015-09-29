ACLOCAL_AMFLAGS =-I ../m4

bin_PROGRAMS = \
	volumereader \
	volumewriter \
	volumewriter_random \
	volumewriter_skipoffset

volumereader_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

volumereader_CPPFLAGS = -I@abs_top_srcdir@/..

volumereader_LDADD = ../youtils/.libs/libyoutils.a $(BUILDTOOLS_LIBS)

volumereader_LDFLAGS = -Wl,--as-needed

volumereader_SOURCES = \
	DeviceReader.cpp \
	DeviceWriter.cpp \
	PatternStrategy.cpp \
	RandomDeviceReader.cpp \
	RandomStrategy.cpp \
	Strategy.cpp \
	VolumeReader.cpp \
	VTException.cpp

volumewriter_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

volumewriter_CPPFLAGS = -I@abs_top_srcdir@/..

volumewriter_LDADD = ../youtils/.libs/libyoutils.a $(BUILDTOOLS_LIBS)

volumewriter_LDFLAGS = -Wl,--as-needed

volumewriter_SOURCES = \
	DeviceWriter.cpp \
	Strategy.cpp \
	RandomStrategy.cpp \
	VTException.cpp \
	PatternStrategy.cpp \
	VolumeWriter.cpp

volumewriter_random_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

volumewriter_random_CPPFLAGS = -I@abs_top_srcdir@/..

volumewriter_random_LDADD =  ../youtils/.libs/libyoutils.a $(BUILDTOOLS_LIBS)

volumewriter_random_LDFLAGS = -Wl,--as-needed

volumewriter_random_SOURCES = \
	VolumeWriterRandom.cpp \
	DeviceWriter.cpp \
	Strategy.cpp \
	RandomStrategy.cpp \
	VTException.cpp \
	PatternStrategy.cpp

volumewriter_skipoffset_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

volumewriter_skipoffset_CPPFLAGS = -I@abs_top_srcdir@/..

volumewriter_skipoffset_LDADD = ../youtils/.libs/libyoutils.a $(BUILDTOOLS_LIBS)

volumewriter_skipoffset_LDFLAGS = -Wl,--as-needed
volumewriter_skipoffset_SOURCES = \
	VolumeWriterSkipOffset.cpp \
	DeviceWriter.cpp \
	Strategy.cpp \
	RandomStrategy.cpp \
	VTException.cpp \
	PatternStrategy.cpp

volumetesterpythondir = @prefix@/lib/python@PYTHON_VERSION@/dist-packages/volumedrivertest/volumetester

volumetesterpython_PYTHON = \
	__init__.py \
	VolumeTester.py

noinst_DATA = pylint_test.sh

TESTS = pylint_test.sh
