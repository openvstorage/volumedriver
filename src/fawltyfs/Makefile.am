ACLOCAL_AMFLAGS = -I ../m4

lib_LTLIBRARIES = libfawltyfs.la

libfawltyfs_la_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
libfawltyfs_la_CPPFLAGS = -I@abs_top_srcdir@/.. -D_FILE_OFFSET_BITS=64
libfawltyfs_la_LDFLAGS = -static -lgcrypt

libfawltyfs_la_SOURCES = \
	ErrorRules.cpp \
	FileSystemCall.cpp \
	FileSystem.cpp

SUBDIRS = . test pyfawltyfs corba-daemon
