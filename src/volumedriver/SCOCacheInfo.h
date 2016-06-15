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
    SCOCacheMountPointInfo(const fs::path& ipath,
                           uint64_t icapacity,
                           uint64_t ifree,
                           uint64_t iused,
                           bool ichoking)
        : path(ipath)
        , capacity(icapacity)
        , free(ifree)
        , used(iused)
        , choking(ichoking)
        , offlined(false)
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

    const fs::path path;
    const uint64_t capacity;
    const uint64_t free;
    const uint64_t used;
    const bool choking;
    const bool offlined;
};

typedef std::map<fs::path, SCOCacheMountPointInfo> SCOCacheMountPointsInfo;
typedef std::map<std::string, SCOCacheNamespaceInfo> SCOCacheNamespacesInfo;

}
#endif // !SCOCACHE_INFO_H_


// Local Variables: **
// mode: c++ **
// End: **
