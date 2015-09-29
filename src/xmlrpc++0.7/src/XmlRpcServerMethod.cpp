
#include "XmlRpcServerMethod.h"
#include "ServerBase.h"

namespace XmlRpc {


  XmlRpcServerMethod::XmlRpcServerMethod(std::string const& name,
                                         xmlrpc::ServerBase* server)
  {
    _name = name;
    _server = server;
    if (_server)
        _server->addMethod(this);
  }

  XmlRpcServerMethod::~XmlRpcServerMethod()
  {
    if (_server)
        _server->removeMethod(this);
  }


} // namespace XmlRpc
