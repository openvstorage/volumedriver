ACLOCAL_AMFLAGS=-I ../../m4
OMNIIDL=@omniidl@

BUILT_SOURCES=corba_youtils.cpp

CLEANFILES=${BUILT_SOURCES}

corba_youtils.cpp : corba_youtils.idl
	$(OMNIIDL) -bcxx -Wbh=.h -Wbs=.cpp -Wbtp $<

bin_PROGRAMS = \
	corba_youtils_server \
	corba_youtils_client

corba_youtils_server_SOURCES = \
	corba_youtils_server.cpp \
	corba_youtils.cpp

# ServerInterfaceImplementation.cpp
# serverinterface.cpp
# SharedMemServer.cpp


corba_youtils_server_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
corba_youtils_server_LDADD =  ../libyoutils.la $(BUILDTOOLS_LIBS)

corba_youtils_client_SOURCES = \
	corba_youtils_client.cpp \
	corba_youtils.cpp \
	TestStop.cpp

# serverinterface.cpp
# SharedMemClient.cpp

corba_youtils_client_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
corba_youtils_client_LDADD = ../libyoutils.la $(BUILDTOOLS_LIBS)


