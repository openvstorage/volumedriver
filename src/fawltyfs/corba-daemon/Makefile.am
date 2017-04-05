ACLOCAL_AMFLAGS=-I ../../m4
OMNIIDL=@omniidl@

BUILT_SOURCES=fawltySK.cc fawlty_idl.py

CLEANFILES=${BUILT_SOURCES}

fawltySK.cc : fawlty.idl
	$(OMNIIDL) -bcxx -Wbtp $<

fawlty_idl.py : fawlty.idl
	$(OMNIIDL) -bpython $<

bin_PROGRAMS= \
	corba_server \
	corba_client

corba_server_SOURCES = \
	corba_server.cpp \
	fawltySK.cc \
	FawltyImplementation.cpp \
	CorbaFileSystem.cpp \
	Literals.cpp

corba_server_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

corba_server_LDADD = \
	../libfawltyfs.la \
	../../youtils/libyoutils.la \
	-lfuse -lgcrypt \
	$(BUILDTOOLS_LIBS)

corba_server_CPPFLAGS = -I@abs_top_srcdir@/..

corba_client_SOURCES = \
	corba_client.cpp \
	fawltySK.cc \
	CorbaClientTest.cpp \
	Literals.cpp

corba_client_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

corba_client_LDADD = \
	../libfawltyfs.la \
	../../youtils/libyoutils.la \
	-lfuse -lgcrypt \
	$(BUILDTOOLS_LIBS)

corba_client_CPPFLAGS = -I@abs_top_srcdir@/..
