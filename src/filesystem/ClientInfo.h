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

#ifndef VFS_CLIENT_INFO_H_
#define VFS_CLIENT_INFO_H_

#include "Object.h"

#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/serialization/optional.hpp>

#include <youtils/OurStrongTypedef.h>
#include <youtils/Serialization.h>

OUR_STRONG_ARITHMETIC_TYPEDEF(uint64_t, ClientInfoTag, volumedriverfs)

namespace volumedriverfs
{

struct ClientInfo
{
    typedef boost::archive::xml_oarchive oarchive_type;
    typedef boost::archive::xml_iarchive iarchive_type;

    boost::optional<ObjectId> object_id;
    std::string ip;
    uint16_t port;

    ClientInfo(boost::optional<ObjectId> id_,
               std::string ip_,
               uint16_t port_)
        : object_id(std::move(id_))
        , ip(std::move(ip_))
        , port(port_)
    {}

    ClientInfo()
        : port(0)
    {}

    template<typename Archive>
    void
    serialize(Archive& ar, const unsigned int /*version*/)
    {
        ar & BOOST_SERIALIZATION_NVP(object_id);
        ar & BOOST_SERIALIZATION_NVP(ip);
        ar & BOOST_SERIALIZATION_NVP(port);
    }

    static constexpr const char* serialization_name = "ClientInfo";

    void
    set_from_str(const std::string& str)
    {
        std::stringstream ss(str);
        iarchive_type ia(ss);
        ia & boost::serialization::make_nvp(serialization_name,
                                            *this);
    }

    std::string
    str() const
    {
        std::stringstream ss;
        youtils::Serialization::serializeNVPAndFlush<oarchive_type>(ss,
                                                                    serialization_name,
                                                                    *this);
        return ss.str();
    }
};

}

BOOST_CLASS_VERSION(volumedriverfs::ClientInfo, 1);

#endif
