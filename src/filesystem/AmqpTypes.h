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

#ifndef VFS_AMQP_URI_H_
#define VFS_AMQP_URI_H_

#include <youtils/InitializedParam.h>
#include <youtils/StrongTypedString.h>

STRONG_TYPED_STRING(volumedriverfs, AmqpUri);
STRONG_TYPED_STRING(volumedriverfs, AmqpExchange);
STRONG_TYPED_STRING(volumedriverfs, AmqpRoutingKey);

namespace volumedriverfs
{

using AmqpUris = std::vector<AmqpUri>;

std::ostream&
operator<<(std::ostream& os,
           const AmqpUris& uris);

}

namespace initialized_params
{

template<>
struct PropertyTreeVectorAccessor<volumedriverfs::AmqpUri>
{
    using Type = volumedriverfs::AmqpUri;

    static const std::string key;

    static Type
    get(const boost::property_tree::ptree& pt)
    {
        return Type(pt.get<std::string>(key));
    }

    static void
    put(boost::property_tree::ptree& pt,
        const Type& uri)
    {
        pt.put(key, uri);
    }
};

}

#endif
