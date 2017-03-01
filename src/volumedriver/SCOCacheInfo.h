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

#ifndef SCOCACHE_INFO_H_
#define SCOCACHE_INFO_H_

// this file is part of the external API.
// Z42: hide boost?

#include <map>
#include <string>
#include <stdint.h>

#include <boost/filesystem.hpp>

#include <youtils/Serialization.h>

#include <backend/Namespace.h>

namespace volumedriver
{

namespace fs = boost::filesystem;

struct SCOCacheNamespaceInfo
{
    SCOCacheNamespaceInfo(const backend::Namespace& iname,
                          uint64_t imin,
                          uint64_t imax_non_disposable)
	: name(iname)
	, min(imin)
        , max_non_disposable(imax_non_disposable)
        , disposable(0)
        , nondisposable(0)
    {}

    const backend::Namespace name;
    const uint64_t min;
    const uint64_t max_non_disposable;
    uint64_t disposable;
    uint64_t nondisposable;
};

struct SCOCacheMountPointInfo
{
    fs::path path;
    uint64_t capacity;
    uint64_t free;
    uint64_t used;
    bool choking;
    bool offlined;

    SCOCacheMountPointInfo(const fs::path& ipath,
                           uint64_t icapacity,
                           uint64_t ifree,
                           uint64_t iused,
                           bool ichoking,
                           bool iofflined = false)
        : path(ipath)
        , capacity(icapacity)
        , free(ifree)
        , used(iused)
        , choking(ichoking)
        , offlined(iofflined)
    {}

    SCOCacheMountPointInfo(const fs::path& ipath,
                           bool iofflined)
        : path(ipath)
        , capacity(0)
        , free(0)
        , used(0)
        , choking(false)
        , offlined(iofflined)
    {}

    bool
    operator==(const SCOCacheMountPointInfo& other) const
    {
#define EQ(x)                                   \
        x == other.x

        return
            EQ(path) and
            EQ(capacity) and
            EQ(free) and
            EQ(used) and
            EQ(choking) and
            EQ(offlined)
            ;
#undef EQ
    }

    bool
    operator!=(const SCOCacheMountPointInfo& other) const
    {
        return not operator==(other);
    }

    template<typename Archive>
    void
    serialize(Archive& ar,
              const unsigned /* version */)
    {
        ar & BOOST_SERIALIZATION_NVP(path);
        ar & BOOST_SERIALIZATION_NVP(capacity);
        ar & BOOST_SERIALIZATION_NVP(free);
        ar & BOOST_SERIALIZATION_NVP(used);
        ar & BOOST_SERIALIZATION_NVP(choking);
        ar & BOOST_SERIALIZATION_NVP(offlined);
    }
};

typedef std::map<fs::path, SCOCacheMountPointInfo> SCOCacheMountPointsInfo;
typedef std::map<std::string, SCOCacheNamespaceInfo> SCOCacheNamespacesInfo;

}

BOOST_CLASS_VERSION(volumedriver::SCOCacheMountPointInfo, 1);

#endif // !SCOCACHE_INFO_H_

// Local Variables: **
// mode: c++ **
// End: **
