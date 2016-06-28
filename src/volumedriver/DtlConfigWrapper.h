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

#ifndef VD_DTL_CONFIG_WRAPPER_H_
#define VD_DTL_CONFIG_WRAPPER_H_

#include "DtlConfig.h"

#include <boost/optional.hpp>

#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

namespace volumedriver
{

// This one exists purely for historical reasons - to keep the serialization backward
// compatible. Get rid of it eventually.
class DtlConfigWrapper
{
public:
    typedef boost::archive::text_iarchive iarchive_type;
    typedef boost::archive::text_oarchive oarchive_type;
    static const std::string config_backend_name;

    enum class CacheType
    {
        None,
        Remote
    };

    DtlConfigWrapper() = default;

    ~DtlConfigWrapper() = default;

    DtlConfigWrapper(const DtlConfigWrapper&) = default;

    DtlConfigWrapper&
    operator=(const DtlConfigWrapper&) = default;

    DtlConfigWrapper(DtlConfigWrapper&&) = default;

    DtlConfigWrapper&
    operator=(DtlConfigWrapper&&) = default;

    CacheType
    getCacheType() const
    {
        return config_ ?
            CacheType::Remote :
            CacheType::None;
    }

    const std::string&
    getCacheHost() const
    {
        static const std::string empty;
        return config_ ?
            config_->host :
            empty;
    }

    uint16_t
    getCachePort() const
    {
        return config_ ?
            config_->port :
            0;
    }

    const boost::optional<DtlConfig>&
    config() const
    {
        return config_;
    }

    void
    set(const boost::optional<DtlConfig>& config)
    {
        config_ = config;
    }

    bool
    operator==(const DtlConfigWrapper& other) const
    {
        return config_ == other.config_;
    }

    bool
    operator!=(const DtlConfigWrapper& other) const
    {
        return not operator==(other);
    }

private:
    boost::optional<DtlConfig> config_;

    friend class boost::serialization::access;
    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        if (version < 1)
        {
            CacheType t;
            ar & t;

            std::string h;
            ar & h;

            uint16_t p;
            ar & p;

            if (t == CacheType::Remote)
            {
                config_ = DtlConfig(h,
                                              p,
                                              DtlMode::Asynchronous);
            }
            else
            {
                config_ = boost::none;
            }
        }
        else
        {
            ar & config_;
        }
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned int /* version */) const
    {
        ar & config_;
    }
};

inline std::ostream&
operator<<(std::ostream& os,
           const DtlConfigWrapper::CacheType& type)
{
    switch(type)
    {
    case DtlConfigWrapper::CacheType::None:
        return os << "None";
    case DtlConfigWrapper::CacheType::Remote:
        return os << "Remote";
    default:
        return os << "huh, you must be insane";
    }
}

}

BOOST_CLASS_VERSION(volumedriver::DtlConfigWrapper, 1);

#endif // !VD_DTL_CONFIG_WRAPPER_H_
