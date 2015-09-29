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

#include "ArakoonNodeConfig.h"
#include "StreamUtils.h"

namespace arakoon
{

std::ostream&
operator<<(std::ostream& os,
           const ArakoonNodeConfig& cfg)
{
    return os << "(" << cfg.node_id_ << ", " << cfg.hostname_ << ", " << cfg.port_ << ")";
}

std::ostream&
operator<<(std::ostream& os,
           const ArakoonNodeConfigs& cfgs)
{
    return youtils::StreamUtils::stream_out_sequence(os,
                                                     cfgs);
}

}

namespace initialized_params
{

using Accessor = PropertyTreeVectorAccessor<arakoon::ArakoonNodeConfig>;

const std::string
Accessor::node_id_key("node_id");

const std::string
Accessor::host_key("host");

const std::string
Accessor::port_key("port");

}
