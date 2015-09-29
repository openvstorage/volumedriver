# makefile written for gnu make
CXX		= g++
SRC		= ./src
CPPFLAGS	= -I$(SRC) -I../include
DEBUG		= -g
OPTIMIZE	= -O0
GCCWARN		= -Wall
CXXFLAGS	= $(DEBUG) $(GCCWARN) $(OPTIMIZE) $(INCLUDES)

LIB		= ./libXmlRpc.a
# Linux: none
# Solaris: -lsocket -lnsl
#SYSTEMLIBS	= -lsocket -lnsl
SYSTEMLIBS	=
LDLIBS		= $(LIB) $(SYSTEMLIBS)

OBJ		= $(SRC)/XmlRpcClient.o $(SRC)/XmlRpcDispatch.o \
		$(SRC)/XmlRpcServer.o $(SRC)/XmlRpcServerConnection.o \
		$(SRC)/XmlRpcServerMethod.o $(SRC)/XmlRpcSocket.o $(SRC)/XmlRpcSource.o \
		$(SRC)/XmlRpcUtil.o $(SRC)/XmlRpcValue.o $(SRC)/XMLRPCServer.o \
                $(SRC)/XMLRPCConnection.o

all:		$(LIB) # tests

$(LIB):		$(OBJ)
		$(AR) $(ARFLAGS) $(LIB) $(OBJ)


tests:		$(LIB)
		cd test && $(MAKE) CXX=$(CXX) CXXFLAGS="$(CXXFLAGS)" SYSTEMLIBS="$(SYSTEMLIBS)"

doc doxygen:
		cd src && doxygen Doxyfile

clean:
		rm -f $(SRC)/*.o
		rm -f $(SRC)/*~
		rm -f $(LIB)
		cd test && $(MAKE) clean

