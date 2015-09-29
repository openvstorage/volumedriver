ACLOCAL_AMFLAGS = -I ../m4

lib_LTLIBRARIES = libsimplenfs.la

libsimplenfs_la_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
libsimplenfs_la_CPPFLAGS = -I@abs_top_srcdir@/..
libsimplenfs_la_LDFLAGS = -static

libsimplenfs_la_SOURCES = \
	File.cpp \
	Mount.cpp

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = simplenfs.pc
