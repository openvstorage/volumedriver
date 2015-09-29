ACLOCAL_AMFLAGS=-I m4
bin_PROGRAMS=volumedriver_example
VOLUMEDRIVER_BUILD_DIR=@VOLUMEDRIVER_BUILD_DIR@
VOLUMEDRIVER_DIR=@VOLUMEDRIVER_DIR@
volumedriver_example_CXXFLAGS=$(BUILDTOOLS_CFLAGS)
volumedriver_example_LDADD=$(VOLUMEDRIVER_BUILD_DIR)/build/volumedriver/libvolumedriver.la \
	$(VOLUMEDRIVER_BUILD_DIR)/build/backend/libbackend.la \
	$(VOLUMEDRIVER_BUILD_DIR)/build/youtils/libyoutils.la \
	$(BUILDTOOLS_LIBS)

volumedriver_example_LDFLAGS=-Wl,--as-needed
volumedriver_example_CPPFLAGS=-I$(VOLUMEDRIVER_DIR)/src

volumedriver_example_SOURCES=example.cpp
