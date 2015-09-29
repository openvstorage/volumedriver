ACLOCAL_AMFLAGS = -I ../../m4
bin_PROGRAMS = compilerlab


compilerlab_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
compilerlab_LDFLAGS = -Wl,--as-needed
compilerlab_LDADD = ../libyoutils.la $(BUILDTOOLS_LIBS)

compilerlab_SOURCES = \
	CompilerLabMain.cpp \
	CompilerLab.cpp


noinst_SCRIPT = runcompilerlab.sh

CLEANFILE = $(noinst_SCRIPT)
