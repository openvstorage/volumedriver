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
