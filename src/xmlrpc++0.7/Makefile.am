ACLOCAL_AMFLAGS=-I ../m4
lib_LTLIBRARIES=libxmlrpc.la

libxmlrpc_la_CXXFLAGS=$(BUILDTOOLS_CFLAGS)
libxmlrpc_la_CPPFLAGS=-I@abs_top_srcdir@/..
libxmlrpc_la_LDFLAGS = -static

libxmlrpc_la_SOURCES= src/ServerBase.cpp src/MTServer.cpp src/XmlRpcClient.cpp src/XmlRpcDispatch.cpp src/XmlRpcServer.cpp \
	src/XmlRpcServerConnection.cpp src/XmlRpcServerMethod.cpp src/XmlRpcSocket.cpp src/XmlRpcSource.cpp src/XmlRpcUtil.cpp \
	src/XmlRpcValue.cpp src/ConnectionBase.cpp src/MTConnection.cpp

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = xmlrpc.pc

SUBDIRS = .
