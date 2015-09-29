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

#ifndef FAILOVERCACHECONFIG_H_
#define FAILOVERCACHECONFIG_H_

#include <iosfwd>

#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/version.hpp>

namespace volumedriver
{

struct FailOverCacheConfig
{
    std::string host;
    uint16_t port;

    FailOverCacheConfig(const std::string& h,
                        const uint16_t p)
        : host(h)
        , port(p)
    {}

    ~FailOverCacheConfig() = default;

    FailOverCacheConfig(const FailOverCacheConfig&) = default;

    FailOverCacheConfig&
    operator=(const FailOverCacheConfig&) = default;

    FailOverCacheConfig(FailOverCacheConfig&& other)
        : host(std::move(other.host))
        , port(other.port)
    {}

    FailOverCacheConfig&
    operator=(FailOverCacheConfig&& other)
    {
        if (this != &other)
        {
            host = std::move(other.host);
            port = other.port;
        }

        return *this;
    }

    bool
    operator==(const FailOverCacheConfig& other) const
    {
        return host == other.host and
            port == other.port;
    }

    bool
    operator!=(const FailOverCacheConfig& other) const
    {
        return not operator==(other);
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int /* version */)
    {
        ar & host;
        ar & port;
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned int /* version */) const
    {
        ar & host;
        ar & port;
    }
};

std::ostream&
operator<<(std::ostream& os,
           const FailOverCacheConfig& cfg);

std::istream&
operator>>(std::istream& is,
           FailOverCacheConfig& cfg);

}

namespace boost
{

namespace serialization
{

template<typename Archive>
inline void
load_construct_data(Archive& /* ar */,
                    volumedriver::FailOverCacheConfig* config,
                    const unsigned /* version */)
{
    new(config) volumedriver::FailOverCacheConfig("",
                                                  0);
}

}

}

BOOST_CLASS_VERSION(volumedriver::FailOverCacheConfig, 0);

#endif // FAILOVERCACHECONFIG_H_

// Local Variables: **
// mode: c++ **
// End: **
