# find a better name + move the C++ source files to a distinct directory
AC_LOCAL_AMFLAGS = -I ../m4

Backenddir=@prefix@/lib/python@PYTHON_VERSION@/dist-packages/volumedriver/backend

Backend_LTLIBRARIES = Backend.la

noinst_DATA=runpython.sh

Backend_la_SOURCES = \
	Backend.cpp

Backend_la_LDFLAGS = -module -shared

Backend_la_LIBADD = \
	../backend/libbackend.la \
	../youtils/libyoutils.la \
	$(BUILDTOOLS_LIBS) \
	-lpthread -lrt -lm -lgcc

Backend_la_CPPFLAGS = -I@abs_top_srcdir@/..
Backend_la_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

Backend_PYTHON = \
	__init__.py

SUBDIRS = . test
