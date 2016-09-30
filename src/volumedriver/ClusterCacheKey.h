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

#ifndef VD_CLUSTER_CACHE_KEY_H_
#define VD_CLUSTER_CACHE_KEY_H_

#include "ClusterCacheHandle.h"
#include "Types.h"

#include <youtils/Serialization.h>
#include <youtils/Md5.h>

namespace volumedriver
{

class ClusterCacheKey
{
public:
    explicit ClusterCacheKey(const youtils::Weed& w)
        : weed_(w)
    {}

    ClusterCacheKey(const ClusterCacheHandle tag,
                    const ClusterAddress ca)
        : location_key_(ca, tag)
    {}

    ~ClusterCacheKey() = default;

    ClusterCacheKey(const ClusterCacheKey& other)
        : location_key_(other.location_key_)
    {}

    ClusterCacheKey&
    operator=(const ClusterCacheKey& other)
    {
        if (this != &other)
        {
            location_key_ = other.location_key_;
        }

        return *this;
    }

    bool
    operator==(const ClusterCacheKey& other) const
    {
        return location_key_.ca == other.location_key_.ca and
            location_key_.tag == other.location_key_.tag;
    }

    bool
    operator!=(const ClusterCacheKey& other) const
    {
        return not operator==(other);
    }

    bool
    operator<(const ClusterCacheKey& other) const
    {
        return weed_ < other.weed_;
    }

    bool
    operator>(const ClusterCacheKey& other) const
    {
        return weed_ > other.weed_;
    }

    const youtils::Weed&
    weed() const
    {
        return weed_;
    }

    ClusterCacheHandle
    cluster_cache_handle() const
    {
        return location_key_.tag;
    }

    ClusterAddress
    cluster_address() const
    {
        return location_key_.ca;
    }

private:
    DECLARE_LOGGER("ClusterCacheKey");

    struct LocationKey
    {
        LocationKey(ClusterAddress a,
                    ClusterCacheHandle o)
            : ca(a)
            , tag(o)
        {}

        ClusterAddress ca;
        ClusterCacheHandle tag;
    };

    union
    {
        youtils::Weed weed_;
        LocationKey location_key_;
    };

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    friend class boost::serialization::access;

    template<class Archive>
    void
    load(Archive& ar, const unsigned int /* version */)
    {
        ar & weed_;
    }

    template<class Archive>
    void save(Archive& ar, const unsigned int version) const
    {
        if (version != 0)
        {
            THROW_SERIALIZATION_ERROR(version, 0, 0);
        }

        ar & weed_;
    }
};

static_assert(sizeof(ClusterCacheKey) == sizeof(youtils::Weed),
              "ClusterCacheKey size badness");

}

BOOST_CLASS_VERSION(volumedriver::ClusterCacheKey, 0);

#endif // !VD_CLUSTER_CACHE_KEY_H_
