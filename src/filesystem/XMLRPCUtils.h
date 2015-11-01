// Copyright 2015 iNuron NV
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

#ifndef VFS_XMLRPC_UTILS_H_
#define VFS_XMLRPC_UTILS_H_

#include <xmlrpc++0.7/src/XmlRpc.h>

#include <youtils/IOException.h>

namespace volumedriverfs
{

struct XMLRPCUtils
{
    DECLARE_LOGGER("XMLRPCUtils");

    static void
    ensure_arg(::XmlRpc::XmlRpcValue& param,
               const std::string& name,
               bool asString = true);

    // the python client (used to) stringify everything.
    template<typename BooleanEnum>
    static BooleanEnum
    get_boolean_enum(::XmlRpc::XmlRpcValue& params,
                     const std::string& name)
    {
        ensure_arg(params, name);
        XmlRpc::XmlRpcValue param = params[name];

        std::string bl(param);
        if (bl == "true" || bl == "True")
        {
            return BooleanEnum::T;
        }
        else if(bl == "false" || bl == "False")
        {
            return BooleanEnum::F;
        }
        else
        {
            throw fungi::IOException(("Argument " + bl +
                                      " should be [Tt]rue or [Ff]alse").c_str());
        }
    }

    static void
    put(::XmlRpc::XmlRpcValue& params,
        const std::string& key,
        const bool b)
    {
        params[key] = b ? "true" : "false";
    };

    template<typename BE>
    static void
    put(::XmlRpc::XmlRpcValue& params,
        const std::string& key,
        const BE b)
    {
        // a bit of a hack.
        static_assert(sizeof(b) == sizeof(bool),
                      "this is only intended for boolean enums!");
        return put_bool(b == BE::T);
    };

};

}

#endif // !VFS_XMLRPC_UTILS_H_
