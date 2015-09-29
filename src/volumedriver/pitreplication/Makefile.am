ACLOCAL_AMFLAGS=-I ../../m4

bin_PROGRAMS = \
	backup_volumedriver \
	restore_volumedriver \
	locked_executable

backup_volumedriver_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

backup_volumedriver_LDADD = \
	../libvolumedriver.la  \
	../../backend/libbackend.la \
	../../youtils/libyoutils.la \
	$(BUILDTOOLS_LIBS)

backup_volumedriver_CPPFLAGS = \
	-I@abs_top_srcdir@/.. \
	-I@abs_top_builddir@

backup_volumedriver_LDFLAGS = -Wl,--as-needed

backup_volumedriver_SOURCES = BackupMain.cpp

restore_volumedriver_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
restore_volumedriver_CPPFLAGS = \
	-I@abs_top_srcdir@/.. \
	-I@abs_top_builddir@

restore_volumedriver_LDADD = \
	../libvolumedriver.la  \
	../../backend/libbackend.la \
	../../youtils/libyoutils.la \
	$(BUILDTOOLS_LIBS)

restore_volumedriver_LDFLAGS = -Wl,--as-needed

restore_volumedriver_SOURCES = RestoreMain.cpp

locked_executable_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

locked_executable_CPPFLAGS = -I@abs_top_srcdir@/..

locked_executable_LDADD = \
	../libvolumedriver.la  \
	../../backend/libbackend.la \
	../../youtils/libyoutils.la \
	$(BUILDTOOLS_LIBS)

locked_executable_LDFLAGS = -Wl,--as-needed

locked_executable_SOURCES = LockedExecutable.cpp

pitreplicationpythondir = @prefix@/lib/python@PYTHON_VERSION@/dist-packages/volumedriver/pitreplication

pitreplicationpython_PYTHON = \
	__init__.py \
	PITReplication.py

pitreplicationconfigdir = @prefix@/etc/volumedriver

pitreplicationconfig_DATA = \
	backupConfigTemplate.json

noinst_DATA = pylint_test.sh

# not pitreplication testing, backend stuff needs to be changed.
TESTS =
# pylint_test.sh
