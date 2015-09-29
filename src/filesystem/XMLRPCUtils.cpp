// Copyright 2015 Open vStorage NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
