ACLOCAL_AMFLAGS=-I ../../m4
OMNIIDL=@omniidl@

BUILT_SOURCES=fawltySK.cc fawlty_idl.py

CLEANFILES=${BUILT_SOURCES}

fawltySK.cc : fawlty.idl
	$(OMNIIDL) -bcxx -Wbtp $<

fawlty_idl.py : fawlty.idl
	$(OMNIIDL) -bpython $<

bin_PROGRAMS = \
	corba_server \
	corba_client

# this convenience lib is a pretty convoluted way of
# shutting up this warning (g++ 5.4, Ubuntu 16.04):
# fawltySK.cc:13:20: warning: '_0RL_library_version' defined but not used [-Wunused-variable]
# static const char* _0RL_library_version = omniORB_4_1;
noinst_LIBRARIES = \
	libfawlty_sk.a

libfawlty_sk_a_SOURCES = \
	fawltySK.cc

libfawlty_sk_a_CXXFLAGS = \
	$(BUILDTOOLS_CFLAGS) -Wno-unused-variable

corba_server_SOURCES = \
	corba_server.cpp \
	FawltyImplementation.cpp \
	CorbaFileSystem.cpp \
	Literals.cpp

corba_server_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

corba_server_LDADD = \
	libfawlty_sk.a \
	../libfawltyfs.la \
	../../youtils/libyoutils.la \
	-lfuse \
	$(BUILDTOOLS_LIBS)

corba_server_CPPFLAGS = -I@abs_top_srcdir@/..

corba_client_SOURCES = \
	corba_client.cpp \
	CorbaClientTest.cpp \
	Literals.cpp

corba_client_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

corba_client_LDADD = \
	libfawlty_sk.a \
	../libfawltyfs.la \
	../../youtils/libyoutils.la \
	-lfuse \
	$(BUILDTOOLS_LIBS)

corba_client_CPPFLAGS = -I@abs_top_srcdir@/..
