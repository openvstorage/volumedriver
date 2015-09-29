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
