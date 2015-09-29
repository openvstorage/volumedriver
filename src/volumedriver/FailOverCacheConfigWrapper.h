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

#ifndef VD_FAILOVER_CACHE_CONFIG_WRAPPER_H_
#define VD_FAILOVER_CACHE_CONFIG_WRAPPER_H_

#include "FailOverCacheConfig.h"

#include <boost/optional.hpp>

#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

namespace volumedriver
{

// This one exists purely for historical reasons - to keep the serialization backward
// compatible. Get rid of it eventually.
class FailOverCacheConfigWrapper
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

    FailOverCacheConfigWrapper() = default;

    ~FailOverCacheConfigWrapper() = default;

    FailOverCacheConfigWrapper(const FailOverCacheConfigWrapper&) = default;

    FailOverCacheConfigWrapper&
    operator=(const FailOverCacheConfigWrapper&) = default;

    FailOverCacheConfigWrapper(FailOverCacheConfigWrapper&&) = default;

    FailOverCacheConfigWrapper&
    operator=(FailOverCacheConfigWrapper&&) = default;

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

    const boost::optional<FailOverCacheConfig>&
    config() const
    {
        return config_;
    }

    void
    set(const boost::optional<FailOverCacheConfig>& config)
    {
        config_ = config;
    }

    bool
    operator==(const FailOverCacheConfigWrapper& other) const
    {
        return config_ == other.config_;
    }

    bool
    operator!=(const FailOverCacheConfigWrapper& other) const
    {
        return not operator==(other);
    }

private:
    boost::optional<FailOverCacheConfig> config_;

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
                config_ = FailOverCacheConfig(h,
                                              p);
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
           const FailOverCacheConfigWrapper::CacheType& type)
{
    switch(type)
    {
    case FailOverCacheConfigWrapper::CacheType::None:
        return os << "None";
    case FailOverCacheConfigWrapper::CacheType::Remote:
        return os << "Remote";
    default:
        return os << "huh, you must be insane";
    }
}

}

BOOST_CLASS_VERSION(volumedriver::FailOverCacheConfigWrapper, 1);

#endif // !VD_FAILOVER_CACHE_CONFIG_WRAPPER_H_
