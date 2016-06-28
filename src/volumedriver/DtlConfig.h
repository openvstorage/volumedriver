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

#ifndef DTL_CONFIG_H_
#define DTL_CONFIG_H_

#include "DtlMode.h"

#include <youtils/Assert.h>

#include <iosfwd>

#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/version.hpp>

namespace volumedriver
{

struct DtlConfig
{
    std::string host;
    uint16_t port;
    DtlMode mode;

    DtlConfig(const std::string& h,
              const uint16_t p,
              const DtlMode m)
        : host(h)
        , port(p)
        , mode(m)
    {}

    ~DtlConfig() = default;

    DtlConfig(const DtlConfig&) = default;

    DtlConfig&
    operator=(const DtlConfig&) = default;

    DtlConfig(DtlConfig&& other)
        : host(std::move(other.host))
        , port(other.port)
        , mode(other.mode)
    {}

    DtlConfig&
    operator=(DtlConfig&& other)
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
    operator==(const DtlConfig& other) const
    {
        return host == other.host and
            port == other.port and
            mode == other.mode;
    }

    bool
    operator!=(const DtlConfig& other) const
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
operator<<(std::ostream&,
           const DtlConfig&);

std::istream&
operator>>(std::istream&,
           DtlConfig&);

}

namespace boost
{

namespace serialization
{

template<typename Archive>
inline void
load_construct_data(Archive& /* ar */,
                    volumedriver::DtlConfig* config,
                    const unsigned /* version */)
{
    new(config) volumedriver::DtlConfig("",
                                        0,
                                        volumedriver::DtlMode::Asynchronous);
}

}

}

BOOST_CLASS_VERSION(volumedriver::DtlConfig, 1);

#endif // DTL_CONFIG_H_

// Local Variables: **
// mode: c++ **
// End: **
