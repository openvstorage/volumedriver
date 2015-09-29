#ifndef _XMLRPC_H_
#define _XMLRPC_H_
//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
//
#include <string>
#include "XmlRpcClient.h"
#include "XmlRpcException.h"
#include "XmlRpcServer.h"
#include "XmlRpcServerMethod.h"
#include "XmlRpcValue.h"
#include "XmlRpcUtil.h"

namespace xmlrpc {
  //! Version identifier
  extern const char XMLRPC_VERSION[];

}

#endif // _XMLRPC_H_

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
