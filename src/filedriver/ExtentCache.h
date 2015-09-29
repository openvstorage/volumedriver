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

#ifndef FILE_DRIVER_EXTENT_CACHE_H_
#define FILE_DRIVER_EXTENT_CACHE_H_

#include "Extent.h"
#include "ExtentId.h"

#include <boost/bimap/set_of.hpp>
#include <boost/filesystem.hpp>

#include <youtils/Logging.h>
#include <youtils/LRUCache.h>

namespace filedriver
{

class ExtentCache
{
    typedef youtils::LRUCache<ExtentId, Extent, boost::bimaps::set_of> Cache;

public:
    ExtentCache(const boost::filesystem::path& path,
                uint32_t capacity);

    ~ExtentCache() = default;

    ExtentCache(const ExtentCache&) = delete;

    ExtentCache&
    operator=(const ExtentCache&) = delete;

    uint32_t
    capacity() const
    {
        return cache_.capacity();
    }

    void
    capacity(uint32_t cap)
    {
        cache_.capacity(cap);
    }

    typedef std::function<std::unique_ptr<Extent>(const ExtentId&,
                                                  const boost::filesystem::path&)> PullFun;
    std::shared_ptr<Extent>
    find(const ExtentId& eid,
         PullFun&& fn);

    bool
    erase(const ExtentId& eid)
    {
        return cache_.erase(eid);
    }

private:
    DECLARE_LOGGER("FileDriverExtentCache");

    const boost::filesystem::path path_;
    Cache cache_;

    boost::filesystem::path
    make_path_(const Cache::Key& eid);

    void
    evict_extent_from_cache_(const Cache::Key& eid);
};

}

#endif // !FILE_DRIVER_EXTENT_CACHE_H_
