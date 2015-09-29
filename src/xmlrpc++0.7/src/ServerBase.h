#ifndef _SERVER_BASE_H
#define _SERVER_BASE_H

#include "XmlRpcSource.h"
#include "XmlRpcDispatch.h"

#include <string>
#include <map>

#include <boost/optional.hpp>

namespace XmlRpc
{
// An abstract class supporting XML RPC methods
class XmlRpcServerMethod;

// Class representing argument and result values
class XmlRpcValue;

class XmlRpcServerConnection;

}

namespace xmlrpc {

namespace x = ::XmlRpc;

class ServerBase
{

public:
    //! Create a server object.
    ServerBase();
    //! Destructor.
    virtual ~ServerBase();

    //! Specify whether introspection is enabled or not. Default is not enabled.
    void enableIntrospection(bool enabled=true);

    //! Add a command to the RPC server
    void addMethod(x::XmlRpcServerMethod* method);

    //! Remove a command from the RPC server
    void removeMethod(x::XmlRpcServerMethod* method);

    //! Remove a command from the RPC server by name
    void removeMethod(const std::string& methodName);

    //! Look up a method by name
    x::XmlRpcServerMethod* findMethod(const std::string& name) const;

    //! Create a socket, bind to the specified address (if none: all addresses!) and
    //! port, and set it in listen mode to make it available for clients.
    virtual bool
    bindAndListen(const boost::optional<std::string>& addr,
                  int port,
                  int backlog = 5) = 0;

    //! Process client requests for the specified time
    virtual void
    work(double timeout) = 0;

    //! Temporarily stop processing client requests and exit the work() method.
    virtual void
    exit() = 0;

    //! Close all connections with clients and the socket file descriptor
    virtual void
    shutdown() = 0;

    //! Introspection support
    void listMethods(x::XmlRpcValue& result);

    virtual uint64_t
    num_connections() = 0;

    // Don't do anything in the single threaded case
    virtual void
    stopAcceptingConnections()
    {};


    virtual void
    startAcceptingConnections()
    {};


  protected:
    // Whether the introspection API is supported by this server
    bool _introspectionEnabled;

    // Collection of methods. This could be a set keyed on method name if we wanted...
    typedef std::map< std::string, x::XmlRpcServerMethod* > MethodMap;
    MethodMap _methods;

    // system methods
    x::XmlRpcServerMethod* _listMethods;
    x::XmlRpcServerMethod* _methodHelp;

    //! Accept a client connection request
    virtual void acceptConnection() = 0;

};


}
#endif

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
