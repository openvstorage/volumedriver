#include "ServerBase.h"

#include "XmlRpcServerMethod.h"
#include "XmlRpcSocket.h"
#include "XmlRpcUtil.h"
#include "XmlRpcException.h"
#include "XmlRpcServerConnection.h"

namespace xmlrpc
{


namespace x = ::XmlRpc;


ServerBase::ServerBase()
{
  _introspectionEnabled = false;
  _listMethods = 0;
  _methodHelp = 0;
}

ServerBase::~ServerBase()
{
    _methods.clear();
    delete _listMethods;
    delete _methodHelp;
}

// Add a command to the RPC server
void
ServerBase::addMethod(x::XmlRpcServerMethod* method)
{
  _methods[method->name()] = method;
}

// Remove a command from the RPC server
void
ServerBase::removeMethod(x::XmlRpcServerMethod* method)
{
  MethodMap::iterator i = _methods.find(method->name());
  if (i != _methods.end())
    _methods.erase(i);
}

// Remove a command from the RPC server by name
void
ServerBase::removeMethod(const std::string& methodName)
{
  MethodMap::iterator i = _methods.find(methodName);
  if (i != _methods.end())
    _methods.erase(i);
}


// Look up a method by name
x::XmlRpcServerMethod*
ServerBase::findMethod(const std::string& name) const
{
  MethodMap::const_iterator i = _methods.find(name);
  if (i == _methods.end())
    return 0;
  return i->second;
}

// Introspection support
static const std::string LIST_METHODS("system.listMethods");
static const std::string METHOD_HELP("system.methodHelp");
static const std::string MULTICALL("system.multicall");


// List all methods available on a server
class ListMethods : public x::XmlRpcServerMethod
{
public:
  ListMethods(ServerBase* s) :
      x::XmlRpcServerMethod(LIST_METHODS, s) {}

    void execute(x::XmlRpcValue& /*params*/,
                 x::XmlRpcValue& result)
    {
        _server->listMethods(result);
    }

    std::string help() { return std::string("List all methods available on a server as an array of strings"); }
};


// Retrieve the help string for a named method
class MethodHelp : public x::XmlRpcServerMethod
{
public:
  MethodHelp(ServerBase* s) : x::XmlRpcServerMethod(METHOD_HELP, s) {}

  void execute(x::XmlRpcValue& params, x::XmlRpcValue& result)
  {
    if (params[0].getType() != x::XmlRpcValue::TypeString)
      throw XmlRpcException(METHOD_HELP + ": Invalid argument type");

    x::XmlRpcServerMethod* m = _server->findMethod(params[0]);
    if ( ! m)
      throw XmlRpcException(METHOD_HELP + ": Unknown method name");

    result = m->help();
  }

  std::string help() { return std::string("Retrieve the help string for a named method"); }
};


// Specify whether introspection is enabled or not. Default is enabled.
void
ServerBase::enableIntrospection(bool enabled)
{
  if (_introspectionEnabled == enabled)
    return;

  _introspectionEnabled = enabled;

  if (enabled)
  {
    if ( ! _listMethods)
    {
      _listMethods = new ListMethods(this);
      _methodHelp = new MethodHelp(this);
    } else {
      addMethod(_listMethods);
      addMethod(_methodHelp);
    }
  }
  else
  {
    removeMethod(LIST_METHODS);
    removeMethod(METHOD_HELP);
  }
}


void
ServerBase::listMethods(x::XmlRpcValue& result)
{
  int i = 0;
  result.setSize(_methods.size()+1);
  for (MethodMap::iterator it=_methods.begin(); it != _methods.end(); ++it)
    result[i++] = it->first;

  // Multicall support is built into XmlRpcServerConnection
  result[i] = MULTICALL;
}

}

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
