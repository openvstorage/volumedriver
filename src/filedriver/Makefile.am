ACLOCAL_AMFLAGS = -I ../m4

lib_LTLIBRARIES = libfiledriver.la

libfiledriver_la_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
libfiledriver_la_CPPFLAGS = -I@abs_top_srcdir@/..
libfiledriver_la_LDFLAGS = -static

libfiledriver_la_SOURCES = \
	Container.cpp \
	ContainerManager.cpp \
	Extent.cpp \
	ExtentCache.cpp \
	ExtentId.cpp \
	FileDriverParameters.cpp

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = filedriver.pc

SUBDIRS = . test
