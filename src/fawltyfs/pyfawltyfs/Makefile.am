ACLOCAL_AMFLAGS = -I ../../m4

pyfawltyfsdir=@prefix@/lib/python@PYTHON_VERSION@/dist-packages/volumedriver/fawltyfs
pyfawltyfs_LTLIBRARIES = PyFawltyFS.la

noinst_DATA=runpython.sh \
	python_init \
	abort-fuse.sh

PyFawltyFS_la_SOURCES = \
	PyFawltyFS.cpp

PyFawltyFS_la_LDFLAGS = -module -shared

PyFawltyFS_la_LIBADD = \
	../libfawltyfs.la \
	../../youtils/libyoutils.la \
	-lfuse \
	$(BUILDTOOLS_LIBS) \
	-lpthread -lrt -lm

PyFawltyFS_la_CPPFLAGS = -I@abs_top_srcdir@/..

PyFawltyFS_la_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

pyfawltyfs_PYTHON = \
	__init__.py


SUBDIRS= . test
