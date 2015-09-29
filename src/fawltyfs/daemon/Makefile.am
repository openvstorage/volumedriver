ACLOCAL_AMFLAGS=-I ../../m4
THRIFT=@thrift@

BUILT_SOURCES=Fawlty.cpp Fawlty.h fawlty_constants.cpp fawlty_constants.h fawlty_types.cpp fawlty_types.h

CLEANFILES=$(BUILT_SOURCES) Fawlty_server.skeleton.cpp

$(BUILT_SOURCES) : fawlty.thrift
	$(THRIFT) --gen cpp -out `pwd` -r $<

bin_PROGRAMS = \
	FawltyServer \
	FawltyClient

FawltyServer_SOURCES = \
	Fawlty_server.cpp \
	FawltyService.cpp

nodist_FawltyServer_SOURCES = \
	Fawlty.cpp \
	fawlty_constants.cpp \
	fawlty_types.cpp

FawltyServer_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
FawltyServer_LDADD = ../libfawltyfs.la ../../youtils/libyoutils.la $(BUILDTOOLS_LIBS)
FawltyServer_CPPFLAGS = -I@abs_top_srcdir@/..

FawltyClient_SOURCES = \
	Fawlty_client.cpp \
	FawltyClientTest.cpp

nodist_FawltyClient_SOURCES = \
	Fawlty.cpp \
	fawlty_constants.cpp \
	fawlty_types.cpp

FawltyClient_CXXFLAGS=$(BUILDTOOLS_CFLAGS)
FawltyClient_LDADD = \
	../libfawltyfs.la \
	../../youtils/libyoutils.la \
	-lfuse \
	$(BUILDTOOLS_LIBS)

FawltyClient_CPPFLAGS = -I@abs_top_srcdir@/..
