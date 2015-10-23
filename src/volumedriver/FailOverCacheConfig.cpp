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

#include "FailOverCacheConfig.h"

#include <iostream>

#include <boost/lexical_cast.hpp>

namespace volumedriver
{

std::ostream&
operator<<(std::ostream& os,
           const FailOverCacheConfig& cfg)
{
    return os << "foc://" << cfg.host << ":" << cfg.port << "," << cfg.mode;
}

std::istream&
operator>>(std::istream& is,
           FailOverCacheConfig& cfg)
{
    char prefix[7];
    is.get(prefix, sizeof(prefix));
    if (std::string("foc://") == (const char *) prefix)
    {
        std::getline(is, cfg.host, ':');
        std::string port;
        std::getline(is, port, ',');
        cfg.port = boost::lexical_cast<uint16_t>(port);
        std::string mode;
        std::getline(is, mode);
        cfg.mode = boost::lexical_cast<FailOverCacheMode>(port);
    }
    return is;
}

}
