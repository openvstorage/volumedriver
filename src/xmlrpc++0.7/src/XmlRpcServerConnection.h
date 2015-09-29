#ifndef _XMLRPCSERVERCONNECTION_H_
#define _XMLRPCSERVERCONNECTION_H_
//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
//

#include <string>
#include <youtils/Logging.h>

#include "XmlRpcValue.h"
#include "XmlRpcSource.h"
#include "XmlRpcServer.h"
#include "ConnectionBase.h"

namespace XmlRpc
{
// The server waits for client connections and provides methods
class XmlRpcServerMethod;

  //! A class to handle XML RPC requests from a particular client
class XmlRpcServerConnection : public XmlRpcSource, public xmlrpc::ConnectionBase
{

    // Static data
      static const char METHODNAME_TAG[];
      static const char PARAMS_TAG[];
      static const char PARAMS_ETAG[];
      static const char PARAM_TAG[];
      static const char PARAM_ETAG[];

      static const std::string SYSTEM_MULTICALL;
      static const std::string METHODNAME;
      static const std::string PARAMS;

      static const std::string FAULTCODE;
      static const std::string FAULTSTRING;

    DECLARE_LOGGER("XmlRpcServerConnection");

  public:
      //! Constructor
      XmlRpcServerConnection(int fd,
                             xmlrpc::XmlRpcServer* server,
                             bool deleteOnClose = false);
      //! Destructor
      virtual ~XmlRpcServerConnection();

      // XmlRpcSource interface implementation
      //! Handle IO on the client connection socket.
      //!   @param eventType Type of IO event that occurred. @see XmlRpcDispatch::EventType.
      virtual unsigned handleEvent(unsigned eventType);

      // Whether to keep the current client connection open for further requests
      bool _keepAlive;
  };
} // namespace XmlRpc

#endif // _XMLRPCSERVERCONNECTION_H_
// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
