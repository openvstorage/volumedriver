// Copyright (C) 2016 iNuron NV
//
// This file is part of Open vStorage Open Source Edition (OSE),
// as available from
//
//      http://www.openvstorage.org and
//      http://www.openvstorage.com.
//
// This file is free software; you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
// as published by the Free Software Foundation, in version 3 as it comes in
// the LICENSE.txt file of the Open vStorage OSE distribution.
// Open vStorage is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY of any kind.

#include "XMLRPCUtils.h"

namespace volumedriverfs
{

void
XMLRPCUtils::ensure_arg(::XmlRpc::XmlRpcValue& val,
                        const std::string& name,
                        bool asString)
{
    if(not val.hasMember(name))
    {
        LOG_ERROR("argument " << name << "not present in xmlrpc arguments");
        throw XmlRpc::XmlRpcException(std::string("Argument ") + name + " not present");
    }
    if(asString and
       val[name].getType() != XmlRpc::XmlRpcValue::TypeString)
    {
        LOG_ERROR("argument " << name << "is not a string");
        throw XmlRpc::XmlRpcException(std::string("Argument ") + name + " is not a string");
    }
}

}
