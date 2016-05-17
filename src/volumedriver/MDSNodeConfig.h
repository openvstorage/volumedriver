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

#ifndef VD_MDS_NODE_CONFIG_H_
#define VD_MDS_NODE_CONFIG_H_

#include <iosfwd>
#include <string>

#include <boost/serialization/export.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/version.hpp>

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


#if BOOST_VERSION == 105800
public:
#endif
    // only used for deserialization
    MDSNodeConfig()
        : port_(0)
    {}

#if BOOST_VERSION == 105800
private:
#endif

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
