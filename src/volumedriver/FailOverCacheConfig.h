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

#ifndef FAILOVERCACHECONFIG_H_
#define FAILOVERCACHECONFIG_H_

#include <youtils/Assert.h>

#include "DtlMode.h"

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
    DtlMode mode;

    FailOverCacheConfig(const std::string& h,
                        const uint16_t p,
                        const DtlMode m)
        : host(h)
        , port(p)
        , mode(m)
    {}

    ~FailOverCacheConfig() = default;

    FailOverCacheConfig(const FailOverCacheConfig&) = default;

    FailOverCacheConfig&
    operator=(const FailOverCacheConfig&) = default;

    FailOverCacheConfig(FailOverCacheConfig&& other)
        : host(std::move(other.host))
        , port(other.port)
        , mode(other.mode)
    {}

    FailOverCacheConfig&
    operator=(FailOverCacheConfig&& other)
    {
        if (this != &other)
        {
            host = std::move(other.host);
            port = other.port;
            mode = other.mode;
        }

        return *this;
    }

    bool
    operator==(const FailOverCacheConfig& other) const
    {
        return host == other.host and
               port == other.port and
               mode == other.mode;
    }

    bool
    operator!=(const FailOverCacheConfig& other) const
    {
        return not operator==(other);
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        ar & host;
        ar & port;
        if (version >= 1)
        {
            ar & mode;
        }
        else
        {
            mode = DtlMode::Asynchronous;
        }
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned int /* version */) const
    {
        ar & host;
        ar & port;
        ar & mode;
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
                                                  0,
                                                  volumedriver::DtlMode::Asynchronous);
}

}

}

BOOST_CLASS_VERSION(volumedriver::FailOverCacheConfig, 1);

#endif // FAILOVERCACHECONFIG_H_

// Local Variables: **
// mode: c++ **
// End: **
