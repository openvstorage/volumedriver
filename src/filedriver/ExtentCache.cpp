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

#include "ExtentCache.h"

namespace filedriver
{

namespace fs = boost::filesystem;

ExtentCache::ExtentCache(const boost::filesystem::path& path,
                         uint32_t capacity)
    : path_(path)
    , cache_("FileDriverExtentCache",
             capacity,
             [&](const Cache::Key& eid,
                 const Cache::Value&)
             {
                 evict_extent_from_cache_(eid);
             })
{
    if (not fs::exists(path_))
    {
        LOG_ERROR("Cache dir " << path_ << " does not exist");
        throw fungi::IOException("Extent cache directory does not exist",
                                 path_.string().c_str(),
                                 ENOENT);
    }

    if (not fs::is_directory(path_))
    {
        LOG_ERROR(path_ << " is not a directory");
        throw fungi::IOException("Specified path for extent cache is not a directory",
                                 path_.string().c_str(),
                                 ENOENT);
    }

    // XXX: support restarting with a warm cache?
    for (auto it = fs::directory_iterator(path_);
         it != fs::directory_iterator();
         ++it)
    {
        LOG_WARN("Leftover entry " << *it << " in extent cache - removing it");
        fs::remove_all(*it);
    }
}

fs::path
ExtentCache::make_path_(const Cache::Key& eid)
{
    LOG_TRACE(eid);
    return path_ / eid.str();
}

void
ExtentCache::evict_extent_from_cache_(const Cache::Key& eid)
{
    LOG_INFO("removing extent " << eid << " from cache");
    fs::remove_all(make_path_(eid));
}

std::shared_ptr<Extent>
ExtentCache::find(const ExtentId& eid,
                  PullFun&& fn)
{
    return cache_.find(eid,
                       [&](const Cache::Key& eid)
                       {
                           return fn(eid, make_path_(eid));
                       });
}

}
