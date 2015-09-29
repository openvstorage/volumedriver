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

#ifndef VD_MDS_NODE_CONFIG_H_
#define VD_MDS_NODE_CONFIG_H_

#include <iosfwd>
#include <string>

#include <boost/serialization/export.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/nvp.hpp>

#include <youtils/InitializedParam.h>
#include <youtils/Serialization.h>

namespace volumedriver
{

struct MDSNodeConfig
{
    MDSNodeConfig(const std::string& addr,
                  const uint16_t prt)
        : address_(addr)
        , port_(prt)
    {}

    const std::string&
    address() const
    {
        return address_;
    }

    uint16_t
    port() const
    {
        return port_;
    }

    bool
    operator==(const MDSNodeConfig& other) const
    {
        return address_ == other.address_ and
            port_ == other.port_;
    }

    bool
    operator!=(const MDSNodeConfig& other) const
    {
        return not operator==(other);
    }

    bool
    operator<(const MDSNodeConfig& other) const
    {
        if (address_ < other.address_)
        {
           return true;
        }
        else if (address_ == other.address_ and
                 port_ < other.port_)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

private:
    std::string address_;
    uint16_t port_;

    friend class boost::serialization::access;

    // only used for deserialization
    MDSNodeConfig()
        : port_(0)
    {}

    template<typename A>
    void
    serialize(A& ar,
              const unsigned version)
    {
        if (version != 1)
        {
            // No backward compatibility for now.
            // The below checks are left in place in case we ever want to change that
            // and serve as documentation.
            THROW_SERIALIZATION_ERROR(version, 1, 1);
        }

        ar & BOOST_SERIALIZATION_NVP(address_);
        ar & BOOST_SERIALIZATION_NVP(port_);
    }
};

using MDSNodeConfigs = std::vector<MDSNodeConfig>;

std::ostream&
operator<<(std::ostream& os,
           const MDSNodeConfig& cfg);

std::ostream&
operator<<(std::ostream& os,
           const MDSNodeConfigs& cfgs);

}

BOOST_CLASS_VERSION(volumedriver::MDSNodeConfig, 1);

namespace initialized_params
{

template<>
struct PropertyTreeVectorAccessor<volumedriver::MDSNodeConfig>
{
    using Type = volumedriver::MDSNodeConfig;

    static const std::string host_key;
    static const std::string port_key;

    static Type
    get(const boost::property_tree::ptree& pt)
    {
        return Type(pt.get<std::string>(host_key),
                    pt.get<uint16_t>(port_key));
    }

    static void
    put(boost::property_tree::ptree& pt,
        const Type& node_cfg)
    {
        pt.put(host_key, node_cfg.address());
        pt.put(port_key, node_cfg.port());
    }
};

}

#endif // !VD_MDS_NODE_CONFIG_H_
